// ================================================================================================
// File: game/skyguard/ecs/systems/spatial_streaming_system.h
// Purpose: SkyGuard streaming controller for SpatialIndexV2 sliding window.
// Used by: Game loop (update order before SpatialIndexSystem)
// ================================================================================================
#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <span>
#include <vector>

#include "core/ecs/components/spatial_dirty_tag.h"
#include "core/ecs/components/spatial_id_component.h"
#include "core/ecs/components/spatial_streamed_out_tag.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/render/sprite_bounds.h"
#include "core/ecs/system.h"
#include "core/ecs/world.h"
#include "core/ecs/systems/spatial_index_system.h"
#include "core/log/log_macros.h"

#include "game/skyguard/ecs/components/player_tag_component.h"
#include "game/skyguard/streaming/chunk_content_provider.h"

namespace sf {
    class RenderWindow;
}

namespace game::skyguard::ecs {

    /**
     * @brief Контроллер стриминга для SlidingWindowSpatialIndex (SkyGuard).
     *
     * Контракт:
     *  - Выполняется ДО SpatialIndexSystem::update().
     *  - Перед выгрузкой чанка снимает SpatialIdComponent (detach/unregister на стороне индекса).
     *  - Уничтожение сущностей — deferred (flushDestroyed в конце кадра).
     */
    class SpatialStreamingSystem final : public core::ecs::ISystem {
      public:
        explicit SpatialStreamingSystem(const core::ecs::SpatialIndexSystemConfig& config) noexcept
            : mChunkSizeWorld(config.index.chunkSizeWorld),
              mWindowWidth(config.storage.width),
              mWindowHeight(config.storage.height),
              mMarginChunks(config.hysteresisMarginChunks),
              mMaxLoadsPerFrame(config.maxLoadsPerFrame),
              mMaxUnloadsPerFrame(config.maxUnloadsPerFrame) {
#if !defined(NDEBUG)
            assert(mChunkSizeWorld > 0 && "SpatialStreamingSystem: invalid chunkSizeWorld");
            assert(mWindowWidth > 0 && mWindowHeight > 0 &&
                   "SpatialStreamingSystem: invalid window dimensions");
            assert(mMarginChunks >= 0 &&
                   "SpatialStreamingSystem: hysteresis margin must be non-negative");
            assert(mMarginChunks * 2 < mWindowWidth &&
                   "SpatialStreamingSystem: hysteresis margin exceeds window width");
            assert(mMarginChunks * 2 < mWindowHeight &&
                   "SpatialStreamingSystem: hysteresis margin exceeds window height");
#else
            if (mChunkSizeWorld <= 0 || mWindowWidth <= 0 || mWindowHeight <= 0 ||
                mMarginChunks < 0 || (mMarginChunks * 2) >= mWindowWidth ||
                (mMarginChunks * 2) >= mWindowHeight) {
                LOG_PANIC(core::log::cat::ECS,
                          "SpatialStreamingSystem: invalid streaming configuration");
            }
#endif

            const std::size_t windowSize =
                static_cast<std::size_t>(mWindowWidth) * static_cast<std::size_t>(mWindowHeight);
            mUnloadScratch.reserve(windowSize);
            mLoadScratch.reserve(windowSize);
            mDeferredDestroyReserve = windowSize;

            if (config.maxVisibleSprites > 0) {
                mDestroyScratch.reserve(config.maxVisibleSprites);
                mDeferredDestroyReserve =
                    std::max(mDeferredDestroyReserve, config.maxVisibleSprites);
            }
        }

        void bind(core::ecs::SpatialIndexSystem* spatialSystem,
                  streaming::IChunkContentProvider* provider) noexcept {
            mSpatialSystem = spatialSystem;

            assert(!mInitialized &&
                   "SpatialStreamingSystem: bind() must happen before first update()");

            mContentProvider = (provider != nullptr) ? provider : &mEmptyProvider;

            const std::size_t maxPerChunk = mContentProvider->maxEntitiesPerChunk();
            mMaxEntitiesPerChunk = maxPerChunk;

            // Буфер спавна — cold path (bind/init). В update() — ноль аллокаций.
            if (maxPerChunk == 0u) {
                mSpawnBuffer.clear(); // size=0, capacity не трогаем
                mChunkEntityStride = 0;
                mChunkEntityCounts.clear();
                mChunkEntities.clear();
                return;
            }

            if (mSpawnBuffer.size() < maxPerChunk) {
                try {
                    mSpawnBuffer.resize(maxPerChunk);
                } catch (const std::bad_alloc&) {
                    LOG_PANIC(core::log::cat::ECS,
                              "SpatialStreamingSystem: failed to allocate spawn buffer (maxPerChunk={})",
                              maxPerChunk);
                }
            }

            // ------------------------------------------------------------------------------------
            // Streaming-owned chunk membership table.
            // ВАЖНО: это - НЕ overlap по AABB. 
            // Это "какие сущности были созданы провайдером для чанка".
            // ------------------------------------------------------------------------------------
            const std::size_t windowSize =
                static_cast<std::size_t>(mWindowWidth) * static_cast<std::size_t>(mWindowHeight);

            if (windowSize == 0u) {
                LOG_PANIC(core::log::cat::ECS, "SpatialStreamingSystem: invalid window size");
            }

            if (windowSize > (std::numeric_limits<std::size_t>::max() / maxPerChunk)) {
                LOG_PANIC(core::log::cat::ECS,
                          "SpatialStreamingSystem: membership pool size overflow "
                          "(windowSize={}, maxPerChunk={})",
                          windowSize, maxPerChunk);
            }

            const std::size_t poolSize = windowSize * maxPerChunk;

            try {
                mChunkEntityStride = maxPerChunk;

                if (mChunkEntityCounts.size() != windowSize) {
                    mChunkEntityCounts.assign(windowSize, 0u);
                } else {
                    std::fill(mChunkEntityCounts.begin(), mChunkEntityCounts.end(), 0u);
                }

                if (mChunkEntities.size() != poolSize) {
                    mChunkEntities.assign(poolSize, core::ecs::NullEntity);
                } else {
                    // Не чистим весь poolSize каждый bind(), это может быть дорого.
                    // Корректность обеспечивается mChunkEntityCounts + очисткой при unloadChunk().
                }
            } catch (const std::bad_alloc&) {
                LOG_PANIC(core::log::cat::ECS,
                          "SpatialStreamingSystem: failed to allocate membership pool "
                          "(poolSize={})",
                          poolSize);
            }


        }

        void update(core::ecs::World& world, float dt) override {
            (void) dt;
            if (mSpatialSystem == nullptr) {
                return;
            }

            mSpatialSystem->ensureDestroyConnection(world);

            auto& index = mSpatialSystem->index();
            const core::spatial::ChunkCoord currentOrigin = index.windowOrigin();
            core::spatial::ChunkCoord desiredOrigin = currentOrigin;

            auto view = world.view<game::skyguard::ecs::LocalPlayerTagComponent,
                                   core::ecs::TransformComponent>();

            const auto it = view.begin();
            const bool hasPlayer = (it != view.end());
            if (hasPlayer) {
                const core::ecs::Entity player = *it;
                const auto& transform = view.get<core::ecs::TransformComponent>(player);
                const core::spatial::ChunkCoord focus =
                    computeFocusChunk(transform.position.x, transform.position.y);

                desiredOrigin = computeDesiredOrigin(currentOrigin, focus, index.windowWidth(),
                                                     index.windowHeight());
            }

            if (!mInitialized) {
                if (desiredOrigin.x != currentOrigin.x || desiredOrigin.y != currentOrigin.y) {
                    index.setWindowOrigin(desiredOrigin);
                }
                initializeWindow(world);
                mInitialized = true;
            }

            if (!hasPlayer) {
                return;
            }

            const core::spatial::ChunkCoord updatedOrigin = index.windowOrigin();
            if (desiredOrigin.x == updatedOrigin.x && desiredOrigin.y == updatedOrigin.y) {
                return;
            }

            const std::int32_t stepX = (desiredOrigin.x == updatedOrigin.x)
                                           ? 0
                                           : ((desiredOrigin.x > updatedOrigin.x) ? 1 : -1);
            const std::int32_t stepY = (desiredOrigin.y == updatedOrigin.y)
                                           ? 0
                                           : ((desiredOrigin.y > updatedOrigin.y) ? 1 : -1);

            if (stepX == 0 && stepY == 0) {
                return;
            }

            const core::spatial::ChunkCoord nextOrigin{updatedOrigin.x + stepX,
                                                       updatedOrigin.y + stepY};

            buildWindowDelta(updatedOrigin, nextOrigin, mUnloadScratch, mLoadScratch);

            if (mUnloadScratch.size() > mMaxUnloadsPerFrame ||
                mLoadScratch.size() > mMaxLoadsPerFrame) {
                return;
            }

            for (const auto coord : mUnloadScratch) {
                unloadChunk(world, coord);
            }

            for (const core::ecs::Entity entity : mDestroyScratch) {
                world.destroyDeferred(entity);
            }
            mDestroyScratch.clear();

            index.setWindowOrigin(nextOrigin);

            for (const auto& coord : mLoadScratch) {
                if (!index.setChunkState(coord, core::spatial::ResidencyState::Loaded)) {
                    LOG_PANIC(core::log::cat::ECS,
                              "SpatialStreamingSystem: failed to set chunk Loaded ({}, {})",
                              coord.x, coord.y);
                }
                loadChunk(world, coord);
            }
        }

        void render(core::ecs::World&, sf::RenderWindow&) override {
        }

      private:
        [[nodiscard]] core::spatial::ChunkCoord computeFocusChunk(const float x,
                                                                  const float y) const noexcept {
            const float invChunk = 1.0f / static_cast<float>(mChunkSizeWorld);
            const std::int32_t cx = static_cast<std::int32_t>(std::floor(x * invChunk));
            const std::int32_t cy = static_cast<std::int32_t>(std::floor(y * invChunk));
            return core::spatial::ChunkCoord{cx, cy};
        }

        /// Origin-independent ring-buffer slot for streaming membership table.
        /// Stable across window origin shifts — no remapping needed.
        /// Contract: for any width-consecutive x-coords and height-consecutive y-coords,
        /// returns unique slots in [0, windowWidth * windowHeight).
        [[nodiscard]] std::size_t membershipSlot(
            const core::spatial::ChunkCoord coord) const noexcept {
            const std::int32_t wx = core::spatial::detail::euclideanMod(coord.x, mWindowWidth);
            const std::int32_t wy = core::spatial::detail::euclideanMod(coord.y, mWindowHeight);
            return static_cast<std::size_t>(wy) * static_cast<std::size_t>(mWindowWidth) +
                   static_cast<std::size_t>(wx);
        }

        [[nodiscard]] core::spatial::ChunkCoord
        computeDesiredOrigin(const core::spatial::ChunkCoord currentOrigin,
                             const core::spatial::ChunkCoord focus, const std::int32_t width,
                             const std::int32_t height) const noexcept {
            core::spatial::ChunkCoord desiredOrigin = currentOrigin;

            const std::int32_t innerMinX = currentOrigin.x + mMarginChunks;
            const std::int32_t innerMaxX = currentOrigin.x + width - mMarginChunks - 1;
            const std::int32_t innerMinY = currentOrigin.y + mMarginChunks;
            const std::int32_t innerMaxY = currentOrigin.y + height - mMarginChunks - 1;

            if (focus.x < innerMinX) {
                desiredOrigin.x = focus.x - mMarginChunks;
            } else if (focus.x > innerMaxX) {
                desiredOrigin.x = focus.x - (width - mMarginChunks - 1);
            }

            if (focus.y < innerMinY) {
                desiredOrigin.y = focus.y - mMarginChunks;
            } else if (focus.y > innerMaxY) {
                desiredOrigin.y = focus.y - (height - mMarginChunks - 1);
            }

            return desiredOrigin;
        }

        void buildWindowDelta(const core::spatial::ChunkCoord oldOrigin,
                              const core::spatial::ChunkCoord newOrigin,
                              std::vector<core::spatial::ChunkCoord>& outUnload,
                              std::vector<core::spatial::ChunkCoord>& outLoad) const noexcept {
            outUnload.clear();
            outLoad.clear();

            const std::int32_t dx = newOrigin.x - oldOrigin.x;
            const std::int32_t dy = newOrigin.y - oldOrigin.y;

            if (dx == 0 && dy == 0) {
                return;
            }

#if !defined(NDEBUG)
            assert(dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1 &&
                   "SpatialStreamingSystem: buildWindowDelta expects single-step shift");
#else
            if (dx < -1 || dx > 1 || dy < -1 || dy > 1) {
                LOG_PANIC(core::log::cat::ECS,
                          "SpatialStreamingSystem: invalid window shift ({}, {}) -> ({}, {})",
                          oldOrigin.x, oldOrigin.y, newOrigin.x, newOrigin.y);
            }
#endif

            const std::int32_t oldMinX = oldOrigin.x;
            const std::int32_t oldMinY = oldOrigin.y;
            const std::int32_t oldMaxX = oldOrigin.x + mWindowWidth - 1;
            const std::int32_t oldMaxY = oldOrigin.y + mWindowHeight - 1;

            const std::int32_t newMinX = newOrigin.x;
            const std::int32_t newMinY = newOrigin.y;
            const std::int32_t newMaxX = newOrigin.x + mWindowWidth - 1;
            const std::int32_t newMaxY = newOrigin.y + mWindowHeight - 1;

            // Полоса выгрузки/загрузки по X (одна колонка).
            std::int32_t unloadColX = 0;
            std::int32_t loadColX = 0;
            const bool hasX = (dx != 0);
            if (hasX) {
                unloadColX = (dx > 0) ? oldMinX : oldMaxX;
                loadColX = (dx > 0) ? newMaxX : newMinX;

                for (std::int32_t y = oldMinY; y <= oldMaxY; ++y) {
                    outUnload.push_back(core::spatial::ChunkCoord{unloadColX, y});
                }
                for (std::int32_t y = newMinY; y <= newMaxY; ++y) {
                    outLoad.push_back(core::spatial::ChunkCoord{loadColX, y});
                }
            }

            // Полоса выгрузки/загрузки по Y (одна строка). На диагональном шаге избегаем
            // дублирования углового чанка.
            if (dy != 0) {
                const std::int32_t unloadRowY = (dy > 0) ? oldMinY : oldMaxY;
                for (std::int32_t x = oldMinX; x <= oldMaxX; ++x) {
                    if (hasX && x == unloadColX) {
                        continue;
                    }
                    outUnload.push_back(core::spatial::ChunkCoord{x, unloadRowY});
                }

                const std::int32_t loadRowY = (dy > 0) ? newMaxY : newMinY;
                for (std::int32_t x = newMinX; x <= newMaxX; ++x) {
                    if (hasX && x == loadColX) {
                        continue;
                    }
                    outLoad.push_back(core::spatial::ChunkCoord{x, loadRowY});
                }
            }
        }

        void initializeWindow(core::ecs::World& world) {
            if (mContentProvider == nullptr) {
                mContentProvider = &mEmptyProvider;
            }

            auto& index = mSpatialSystem->index();
            const core::spatial::ChunkCoord origin = index.windowOrigin();
            const std::int32_t width = index.windowWidth();
            const std::int32_t height = index.windowHeight();

            if (mDeferredDestroyReserve > 0) {
                world.reserveDeferredDestroyQueue(mDeferredDestroyReserve);
            }

            for (std::int32_t y = origin.y; y < origin.y + height; ++y) {
                for (std::int32_t x = origin.x; x < origin.x + width; ++x) {
                    const core::spatial::ChunkCoord coord{x, y};
                    if (!index.setChunkState(coord, core::spatial::ResidencyState::Loaded)) {
                        LOG_PANIC(core::log::cat::ECS,
                                  "SpatialStreamingSystem: failed to set chunk Loaded ({}, {})",
                                  coord.x, coord.y);
                    }
                    loadChunk(world, coord);
                }
            }
        }

        void loadChunk(core::ecs::World& world, const core::spatial::ChunkCoord coord) {

            if (mContentProvider == nullptr) {
                mContentProvider = &mEmptyProvider;
            }

            if (mMaxEntitiesPerChunk == 0u) {
                return; // Empty/disabled provider (maxEntitiesPerChunk()==0)
            }

            auto& index = mSpatialSystem->index();
            const auto st = index.chunkState(coord);
            if (st != core::spatial::ResidencyState::Loaded) {
                LOG_PANIC(core::log::cat::ECS,
                          "SpatialStreamingSystem: loadChunk() called for non-Loaded chunk ({}, "
                          "{}) state={}",
                          coord.x, coord.y, static_cast<int>(st));
            }

            const std::size_t count = mContentProvider->fillChunkContent(coord, mSpawnBuffer);
            if (count > mSpawnBuffer.size()) {
                LOG_PANIC(core::log::cat::ECS,
                          "SpatialStreamingSystem: provider overflow (count={}, capacity={})",
                          count, mSpawnBuffer.size());
            }
            if (count == 0u) {
                return;
            }

            const core::spatial::WorldPosf origin =
                core::spatial::chunkOrigin<float>(coord, mChunkSizeWorld);

            const std::size_t slot = membershipSlot(coord);

            if (slot >= mChunkEntityCounts.size()) {
                LOG_PANIC(core::log::cat::ECS,
                          "SpatialStreamingSystem: slot out of range for chunk "
                          "({}, {}) slot={} windowSize={}",
                          coord.x, coord.y, slot, mChunkEntityCounts.size());
            }
            if (mChunkEntityStride == 0u || mChunkEntities.empty()) {
                LOG_PANIC(core::log::cat::ECS,
                          "SpatialStreamingSystem: membership pool not initialized "
                          "(stride=0)");
            }
            if (mChunkEntityCounts[slot] != 0u) {
                LOG_PANIC(core::log::cat::ECS,
                          "SpatialStreamingSystem: double-load detected for chunk "
                          "({}, {}) slot={} count={}",
                          coord.x, coord.y, slot, mChunkEntityCounts[slot]);
            }
            if (count > mChunkEntityStride) {
                LOG_PANIC(core::log::cat::ECS,
                          "SpatialStreamingSystem: provider overflow (count={}, stride={})",
                          count, mChunkEntityStride);
            }

            const std::size_t base = slot * mChunkEntityStride;

            for (std::size_t i = 0; i < count; ++i) {
                const auto& desc = mSpawnBuffer[i];

                // validate-on-write (включая Release): NaN/Inf здесь приведут к мусору в AABB и
                // потенциальному UB в последующих сортировках/сравнениях.
                const bool finite = std::isfinite(desc.localPos.x) &&
                                    std::isfinite(desc.localPos.y) && std::isfinite(desc.scale.x) &&
                                    std::isfinite(desc.scale.y) && std::isfinite(desc.origin.x) &&
                                    std::isfinite(desc.origin.y) && std::isfinite(desc.zOrder);

                if (!finite) {
                    LOG_PANIC(core::log::cat::ECS,
                              "SpatialStreamingSystem: provider produced non-finite values "
                              "chunk=({}, {}), i={}, pos=({},{}) scale=({},{}) origin=({},{}) z={}",
                              coord.x, coord.y, i, desc.localPos.x, desc.localPos.y, desc.scale.x,
                              desc.scale.y, desc.origin.x, desc.origin.y, desc.zOrder);
                }

                if (!desc.texture.valid()) {
                    LOG_PANIC(core::log::cat::ECS,
                              "SpatialStreamingSystem: provider produced invalid TextureKey "
                              "chunk=({}, {}), i={}",
                              coord.x, coord.y, i);
                }

                if (!core::ecs::render::hasExplicitRect(desc.textureRect)) {
                    LOG_PANIC(core::log::cat::ECS,
                              "SpatialStreamingSystem: provider produced non-explicit textureRect "
                              "chunk=({}, {}), i={}",
                              coord.x, coord.y, i);
                }

                const core::ecs::Entity entity = world.createEntity();
                if (entity == core::ecs::NullEntity) {
                    LOG_PANIC(core::log::cat::ECS,
                              "SpatialStreamingSystem: entity creation failed for chunk ({}, {})",
                              coord.x, coord.y);
                }

                core::ecs::TransformComponent tr{};
                tr.position = {origin.x + desc.localPos.x, origin.y + desc.localPos.y};
                tr.rotationDegrees = 0.f;

                core::ecs::SpriteComponent sp{};
                sp.texture = desc.texture;
                sp.textureRect = desc.textureRect;
                sp.scale = desc.scale;
                sp.origin = desc.origin;
                sp.zOrder = desc.zOrder;

                world.addComponent<core::ecs::TransformComponent>(entity, tr);
                world.addComponent<core::ecs::SpriteComponent>(entity, sp);

                // Streaming membership: entity “belongs” to this chunk by construction.
                mChunkEntities[base + i] = entity;
            }

            mChunkEntityCounts[slot] = static_cast<std::uint32_t>(count);
        }

        void unloadChunk(core::ecs::World& world, const core::spatial::ChunkCoord coord) {
            auto& index = mSpatialSystem->index();

            // Membership cleanup — only when provider actually spawns entities.
            if (mMaxEntitiesPerChunk > 0u) {
                const std::size_t slot = membershipSlot(coord);
                assert(slot < mChunkEntityCounts.size() &&
                       "SpatialStreamingSystem: slot out of range for unload");
                assert(mChunkEntityStride > 0 && !mChunkEntities.empty() &&
                       "SpatialStreamingSystem: membership pool not initialized");

                const std::uint32_t count = mChunkEntityCounts[slot];
                const std::size_t base = slot * mChunkEntityStride;

                for (std::uint32_t i = 0; i < count; ++i) {
                    const core::ecs::Entity entity = mChunkEntities[base + i];
                    mChunkEntities[base + i] = core::ecs::NullEntity;

                    if (entity == core::ecs::NullEntity || !world.isAlive(entity)) {
                        continue;
                    }

                    if (!world.hasTag<core::ecs::SpatialStreamedOutTag>(entity)) {
                        world.addTag<core::ecs::SpatialStreamedOutTag>(entity);
                    }

                    if (world.hasComponent<core::ecs::SpatialIdComponent>(entity)) {
                        world.removeComponent<core::ecs::SpatialIdComponent>(entity);
                    }
                    if (world.hasTag<core::ecs::SpatialDirtyTag>(entity)) {
                        world.removeTag<core::ecs::SpatialDirtyTag>(entity);
                    }

                    mDestroyScratch.push_back(entity);
                }

                mChunkEntityCounts[slot] = 0u;
            }

            const bool unloaded = index.setChunkState(
                coord, core::spatial::ResidencyState::Unloaded);
            if (!unloaded) {
                LOG_PANIC(core::log::cat::ECS,
                          "SpatialStreamingSystem: failed to unload chunk ({}, {})",
                          coord.x, coord.y);
            }

            mContentProvider->onChunkUnloaded(coord);
        }

        core::ecs::SpatialIndexSystem* mSpatialSystem{nullptr};
        streaming::IChunkContentProvider* mContentProvider{nullptr};
        streaming::EmptyChunkContentProvider mEmptyProvider{};

        std::int32_t mChunkSizeWorld = 0;
        std::int32_t mWindowWidth = 0;
        std::int32_t mWindowHeight = 0;
        std::int32_t mMarginChunks = 0;
        std::uint32_t mMaxLoadsPerFrame = 0;
        std::uint32_t mMaxUnloadsPerFrame = 0;

        bool mInitialized = false;
        std::size_t mDeferredDestroyReserve = 0;

        std::size_t mMaxEntitiesPerChunk = 0;

        std::vector<core::spatial::ChunkCoord> mUnloadScratch{};
        std::vector<core::spatial::ChunkCoord> mLoadScratch{};
        std::vector<core::ecs::Entity> mDestroyScratch{};
        std::vector<streaming::ChunkEntityDesc> mSpawnBuffer{};
        std::size_t mChunkEntityStride = 0;
        std::vector<std::uint32_t> mChunkEntityCounts{}; // windowSize
        std::vector<core::ecs::Entity> mChunkEntities{}; // windowSize * stride
    };

} // namespace game::skyguard::ecs
