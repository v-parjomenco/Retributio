// ================================================================================================
// File: game/skyguard/ecs/systems/spatial_streaming_system.h
// Purpose: SkyGuard streaming controller for SpatialIndexV2 sliding window.
// Used by: Game loop (update order before SpatialIndexSystem)
// ================================================================================================
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "core/ecs/components/spatial_dirty_tag.h"
#include "core/ecs/components/spatial_id_component.h"
#include "core/ecs/components/spatial_streamed_out_tag.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/system.h"
#include "core/ecs/world.h"
#include "core/ecs/systems/spatial_index_system.h"
#include "core/log/log_macros.h"

#include "game/skyguard/ecs/components/player_tag_component.h"

namespace game::skyguard::ecs {

    /**
     * @brief Streaming controller for SlidingWindowSpatialIndex (SkyGuard).
     *
     * Контракт:
     *  - Выполняется ДО SpatialIndexSystem::update().
     *  - Снимает SpatialIdComponent перед выгрузкой чанка (detach/unregister).
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

            if (config.maxVisibleSprites > 0) {
                mEntityIdsScratch.reserve(config.maxVisibleSprites);
                mDestroyScratch.reserve(config.maxVisibleSprites);
            }
        }

        void bind(core::ecs::SpatialIndexSystem* spatialSystem) noexcept {
            mSpatialSystem = spatialSystem;
            if (mSpatialSystem == nullptr) {
                return;
            }

            auto& index = mSpatialSystem->index();
            const core::spatial::ChunkCoord origin = index.windowOrigin();
            const std::int32_t width = index.windowWidth();
            const std::int32_t height = index.windowHeight();

            for (std::int32_t y = origin.y; y < origin.y + height; ++y) {
                for (std::int32_t x = origin.x; x < origin.x + width; ++x) {
                    const bool loaded =
                        index.setChunkState({x, y}, core::spatial::ResidencyState::Loaded);
                    if (!loaded) {
                        LOG_PANIC(core::log::cat::ECS,
                                  "SpatialStreamingSystem: failed initial load ({}, {})", x, y);
                    }
                }
            }
        }

        void update(core::ecs::World& world, float dt) override {
            (void) dt;
            if (mSpatialSystem == nullptr) {
                return;
            }

            mSpatialSystem->ensureDestroyConnection(world);

            auto view = world.view<game::skyguard::ecs::LocalPlayerTagComponent,
                                   core::ecs::TransformComponent>();

            const auto it = view.begin();
            if (it == view.end()) {
                return;
            }

            const core::ecs::Entity player = *it;
            const auto& transform = view.get<core::ecs::TransformComponent>(player);
            const core::spatial::ChunkCoord focus =
                computeFocusChunk(transform.position.x, transform.position.y);

            auto& index = mSpatialSystem->index();
            const core::spatial::ChunkCoord currentOrigin = index.windowOrigin();

            core::spatial::ChunkCoord desiredOrigin = currentOrigin;
            const std::int32_t width = index.windowWidth();
            const std::int32_t height = index.windowHeight();

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

            if (desiredOrigin.x == currentOrigin.x && desiredOrigin.y == currentOrigin.y) {
                return;
            }

            const std::int32_t stepX = (desiredOrigin.x == currentOrigin.x)
                                           ? 0
                                           : ((desiredOrigin.x > currentOrigin.x) ? 1 : -1);
            const std::int32_t stepY = (desiredOrigin.y == currentOrigin.y)
                                           ? 0
                                           : ((desiredOrigin.y > currentOrigin.y) ? 1 : -1);

            if (stepX == 0 && stepY == 0) {
                return;
            }

            const core::spatial::ChunkCoord nextOrigin{currentOrigin.x + stepX,
                                                       currentOrigin.y + stepY};

            buildWindowDelta(currentOrigin, nextOrigin, mUnloadScratch, mLoadScratch);

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

            for (const auto coord : mLoadScratch) {
                const bool loaded = index.setChunkState(coord, core::spatial::ResidencyState::Loaded);
                if (!loaded) {
                    LOG_PANIC(core::log::cat::ECS,
                              "SpatialStreamingSystem: failed to load chunk ({}, {})",
                              coord.x, coord.y);
                }
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

        void buildWindowDelta(const core::spatial::ChunkCoord oldOrigin,
                              const core::spatial::ChunkCoord newOrigin,
                              std::vector<core::spatial::ChunkCoord>& outUnload,
                              std::vector<core::spatial::ChunkCoord>& outLoad) const noexcept {
            outUnload.clear();
            outLoad.clear();

            const std::int32_t oldMinX = oldOrigin.x;
            const std::int32_t oldMinY = oldOrigin.y;
            const std::int32_t oldMaxX = oldOrigin.x + mWindowWidth - 1;
            const std::int32_t oldMaxY = oldOrigin.y + mWindowHeight - 1;

            const std::int32_t newMinX = newOrigin.x;
            const std::int32_t newMinY = newOrigin.y;
            const std::int32_t newMaxX = newOrigin.x + mWindowWidth - 1;
            const std::int32_t newMaxY = newOrigin.y + mWindowHeight - 1;

            for (std::int32_t y = oldMinY; y <= oldMaxY; ++y) {
                for (std::int32_t x = oldMinX; x <= oldMaxX; ++x) {
                    if (x < newMinX || x > newMaxX || y < newMinY || y > newMaxY) {
                        outUnload.push_back(core::spatial::ChunkCoord{x, y});
                    }
                }
            }

            for (std::int32_t y = newMinY; y <= newMaxY; ++y) {
                for (std::int32_t x = newMinX; x <= newMaxX; ++x) {
                    if (x < oldMinX || x > oldMaxX || y < oldMinY || y > oldMaxY) {
                        outLoad.push_back(core::spatial::ChunkCoord{x, y});
                    }
                }
            }
        }

        void unloadChunk(core::ecs::World& world, const core::spatial::ChunkCoord coord) {
            auto& index = mSpatialSystem->index();
            index.collectEntityIdsInChunk(coord, mEntityIdsScratch);

            const std::span<const core::ecs::Entity> mapping =
                mSpatialSystem->entitiesBySpatialId();

            for (const core::spatial::EntityId32 id : mEntityIdsScratch) {
                if (id >= mapping.size()) {
                    LOG_PANIC(core::log::cat::ECS,
                              "SpatialStreamingSystem: SpatialId32 out of range (id={})", id);
                }

                const core::ecs::Entity entity = mapping[id];
                if (entity == core::ecs::NullEntity || !world.isAlive(entity)) {
                    continue;
                }

                if (!world.hasComponent<core::ecs::SpatialStreamedOutTag>(entity)) {
                    world.addComponent<core::ecs::SpatialStreamedOutTag>(entity);
                }

                if (world.hasComponent<core::ecs::SpatialIdComponent>(entity)) {
                    world.removeComponent<core::ecs::SpatialIdComponent>(entity);
                }
                if (world.hasComponent<core::ecs::SpatialDirtyTag>(entity)) {
                    world.removeComponent<core::ecs::SpatialDirtyTag>(entity);
                }

                mDestroyScratch.push_back(entity);
            }
        }

        core::ecs::SpatialIndexSystem* mSpatialSystem{nullptr};

        std::int32_t mChunkSizeWorld = 0;
        std::int32_t mWindowWidth = 0;
        std::int32_t mWindowHeight = 0;
        std::int32_t mMarginChunks = 0;
        std::uint32_t mMaxLoadsPerFrame = 0;
        std::uint32_t mMaxUnloadsPerFrame = 0;

        std::vector<core::spatial::ChunkCoord> mUnloadScratch{};
        std::vector<core::spatial::ChunkCoord> mLoadScratch{};
        std::vector<core::spatial::EntityId32> mEntityIdsScratch{};
        std::vector<core::ecs::Entity> mDestroyScratch{};
    };

} // namespace game::skyguard::ecs