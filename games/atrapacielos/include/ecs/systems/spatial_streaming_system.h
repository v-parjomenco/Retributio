// ================================================================================================
// File: ecs/systems/spatial_streaming_system.h
// Purpose: Atrapacielos streaming controller for SpatialIndexV2 sliding window.
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

#include "ecs/components/player_tag_component.h"
#include "streaming/chunk_content_provider.h"

namespace sf {
    class RenderWindow;
}

namespace game::atrapacielos::ecs {

    /**
     * @brief Контроллер стриминга для SlidingWindowSpatialIndex (Atrapacielos).
     *
     * Контракт:
     *  - Выполняется ДО SpatialIndexSystem::update().
     *  - Перед выгрузкой чанка снимает SpatialIdComponent (detach на стороне индекса).
     *  - Уничтожение сущностей — deferred (flushDestroyed в конце кадра).
     *
     * Initial load (Начальная загрузка) — двухфазный:
     *  - Phase 1 (SettingLoaded): все чанки окна переводятся в Loaded state БЕЗ создания
     *    сущностей. SpatialIndexSystem не видит новых сущностей → нет registerEntity.
     *    Бюджет: maxLoadsPerFrame чанков/кадр.
     *  - Phase 2 (SpawningEntities): сущности создаются для всех чанков (уже Loaded).
     *    SpatialIndexSystem регистрирует их — canWriteRange() всегда true,
     *    потому что ВСЕ соседние чанки гарантированно Loaded.
     *    Бюджет: maxInitSpawnChunksPerFrame чанков/кадр (учитывает лимит сущностей).
     *  - Пока initial load не завершён, нормальный стриминг (сдвиг окна) заблокирован.
     *
     * Гарантия отсутствия re-registration (streamed-out safety) — тройная защита:
     *  1. EnTT view filter: SpatialIndexSystem::update() использует
     *     entt::exclude<SpatialStreamedOutTag> в ОБОИХ view (newView и dirtyView).
     *     Streamed-out сущности архитектурно невидимы для register/update path.
     *  2. Execution order: SpatialStreamingSystem::update() выполняется ДО
     *     SpatialIndexSystem::update(). unloadChunk() ставит SpatialStreamedOutTag
     *     и снимает SpatialDirtyTag до того, как SpatialIndexSystem получит управление.
     *  3. Debug assert: SpatialIndexSystem содержит assert(!SpatialStreamedOutTag)
     *     в register loop (дополнительная страховка от регрессий при рефакторинге).
     */
    class SpatialStreamingSystem final : public core::ecs::ISystem {
      public:
        /// Фазы initial load. Двухфазный протокол гарантирует, что SpatialIndexSystem
        /// никогда не встретит non-Loaded chunk при registerEntity во время инициализации.
        enum class InitPhase : std::uint8_t {
            NotStarted = 0,

            /// Phase 1: все чанки окна переводятся в Loaded (выделяются cell blocks),
            /// но сущности НЕ создаются. SpatialIndexSystem находится в режиме ожидания.
            SettingLoaded,

            /// Phase 2: сущности создаются для чанков (все уже переведены в Loaded).
            /// SpatialIndexSystem регистрирует их безопасно.
            SpawningEntities,

            /// Initial load завершён. Нормальный стриминг (сдвиг окна) разблокирован.
            Complete
        };

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

            if (windowSize == 0u ||
                windowSize > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
                LOG_PANIC(core::log::cat::ECS,
                          "SpatialStreamingSystem: window size overflow/invalid ({}x{})",
                          mWindowWidth, mWindowHeight);
            }

            mWindowTotalChunks = static_cast<std::uint32_t>(windowSize);

            const std::size_t shiftMax =
                static_cast<std::size_t>(mWindowWidth) +
                static_cast<std::size_t>(mWindowHeight);

            mUnloadScratch.reserve(shiftMax);
            mLoadScratch.reserve(shiftMax);
            mDeferredDestroyReserve = shiftMax;
        }

        void bind(core::ecs::SpatialIndexSystem* spatialSystem,
                  streaming::IChunkContentProvider* provider) noexcept {
            mSpatialSystem = spatialSystem;

            assert(!mInitialized &&
                   "SpatialStreamingSystem: bind() must happen before first update()");

            mContentProvider = (provider != nullptr) ? provider : &mEmptyProvider;

            const std::size_t maxPerChunk = mContentProvider->maxEntitiesPerChunk();
            mMaxEntitiesPerChunk = maxPerChunk;

            // Если провайдер не генерирует сущности, обнуляем пулы.
            if (maxPerChunk == 0u) {
                mSpawnBuffer.clear();
                mChunkEntityStride = 0;
                mChunkEntityCounts.clear();
                mChunkEntities.clear();
                mMaxInitSpawnChunksPerFrame = mMaxLoadsPerFrame;
                return;
            }

            // Подготовка буфера для генерации сущностей в рамках одного чанка.
            if (mSpawnBuffer.size() < maxPerChunk) {
                try {
                    mSpawnBuffer.resize(maxPerChunk);
                } catch (const std::bad_alloc&) {
                    LOG_PANIC(core::log::cat::ECS,
                              "SpatialStreamingSystem: failed to alloc spawn buffer (max={})",
                              maxPerChunk);
                }
            }

            const std::size_t windowSize =
                static_cast<std::size_t>(mWindowWidth) * static_cast<std::size_t>(mWindowHeight);

            if (windowSize == 0u) {
                LOG_PANIC(core::log::cat::ECS, "SpatialStreamingSystem: invalid window size");
            }
            if (windowSize > (std::numeric_limits<std::size_t>::max() / maxPerChunk)) {
                LOG_PANIC(core::log::cat::ECS,
                          "SpatialStreamingSystem: membership pool size overflow "
                          "(windowSize={}, maxPerChunk={})", windowSize, maxPerChunk);
            }

            // Инициализация пула принадлежности сущностей (membership pool).
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
                }
            } catch (const std::bad_alloc&) {
                LOG_PANIC(core::log::cat::ECS,
                          "SpatialStreamingSystem: failed to alloc membership pool (poolSize={})",
                          poolSize);
            }

            // Резервирование памяти для пакета удаляемых сущностей.
            if (mMaxUnloadsPerFrame > 0u) {
                const std::size_t unloadChunks = static_cast<std::size_t>(mMaxUnloadsPerFrame);
                if (unloadChunks > (std::numeric_limits<std::size_t>::max() / maxPerChunk)) {
                    LOG_PANIC(core::log::cat::ECS,
                              "SpatialStreamingSystem: destroy scratch size overflow "
                              "(unloadChunks={}, maxPerChunk={})", unloadChunks, maxPerChunk);
                }
                const std::size_t maxDestroyPerShift = unloadChunks * maxPerChunk;
                if (mDestroyScratch.capacity() < maxDestroyPerShift) {
                    try {
                        mDestroyScratch.reserve(maxDestroyPerShift);
                    } catch (const std::bad_alloc&) {
                        LOG_PANIC(core::log::cat::ECS,
                                  "SpatialStreamingSystem: failed to reserve destroy scratch "
                                  "(max={})", maxDestroyPerShift);
                    }
                }
                mDeferredDestroyReserve = std::max(mDeferredDestroyReserve, maxDestroyPerShift);
            }

            // Бюджет Phase 2: ограничиваем количество чанков так, чтобы не создавать
            // более ~50,000 сущностей за кадр, избегая спайков (freeze) во время загрузки.
            constexpr std::size_t kMaxInitEntitiesPerFrame = 50'000;
            const std::size_t chunksForEntityBudget =
                std::max<std::size_t>(1u, kMaxInitEntitiesPerFrame / maxPerChunk);
            mMaxInitSpawnChunksPerFrame = static_cast<std::uint32_t>(
                std::min<std::size_t>(mMaxLoadsPerFrame, chunksForEntityBudget));

            const std::size_t entitiesBytes = poolSize * sizeof(core::ecs::Entity);
            const std::size_t countsBytes = windowSize * sizeof(std::uint32_t);
            const std::size_t spawnBytes = maxPerChunk * sizeof(streaming::ChunkEntityDesc);
            const std::size_t totalMembershipBytes = entitiesBytes + countsBytes + spawnBytes;

            LOG_INFO(core::log::cat::ECS,
                     "SpatialStreamingSystem: membership pool allocated "
                     "(window={}, stride={}, poolSize={}, "
                     "entities={} KB, counts={} KB, spawnBuf={} KB, total={} KB, "
                     "initSpawnBudget={} chunks/frame)",
                     windowSize, mChunkEntityStride, poolSize,
                     entitiesBytes / 1024u, countsBytes / 1024u,
                     spawnBytes / 1024u, totalMembershipBytes / 1024u,
                     mMaxInitSpawnChunksPerFrame);
        }

        void update(core::ecs::World& world, float dt) override {
            (void) dt;
            if (mSpatialSystem == nullptr) {
                return;
            }

#if !defined(NDEBUG) || defined(RETRIBUTIO_PROFILE)
            mDebugFrameLoads = 0;
            mDebugFrameUnloads = 0;
#endif

            mSpatialSystem->ensureDestroyConnection(world);
            auto& index = mSpatialSystem->index();
            const core::spatial::ChunkCoord currentOrigin = index.windowOrigin();
            core::spatial::ChunkCoord desiredOrigin = currentOrigin;

            // Находим игрока для определения фокуса стриминга.
            auto view = world.view<game::atrapacielos::ecs::LocalPlayerTagComponent,
                                   core::ecs::TransformComponent>();
            const auto it = view.begin();
            const bool hasPlayer = (it != view.end());
            
            if (hasPlayer) {
                const core::ecs::Entity player = *it;
                const auto& transform = view.get<core::ecs::TransformComponent>(player);
                const core::spatial::ChunkCoord focus =
                    computeFocusChunk(transform.position.x, transform.position.y);
                desiredOrigin = computeDesiredOrigin(currentOrigin, focus,
                                                     index.windowWidth(), index.windowHeight());
            }

#if !defined(NDEBUG) || defined(RETRIBUTIO_PROFILE)
            mDebugDesiredOrigin = desiredOrigin;
#endif

            // Если система еще не инициализирована, запускаем процесс загрузки (Phase 1).
            if (!mInitialized) {
                if (desiredOrigin.x != currentOrigin.x || desiredOrigin.y != currentOrigin.y) {
                    index.setWindowOrigin(desiredOrigin);
                }
                beginInitialLoad(world);
                mInitialized = true;
            }

            // Пока Initial Load не завершен, блокируем нормальный сдвиг окна.
            if (mInitPhase != InitPhase::Complete) {
                continueInitialLoad(world);
                return;
            }

            if (!hasPlayer) {
                return;
            }

            const core::spatial::ChunkCoord updatedOrigin = index.windowOrigin();
            if (desiredOrigin.x == updatedOrigin.x && desiredOrigin.y == updatedOrigin.y) {
                return;
            }

            // Вычисляем шаг сдвига окна (максимум на 1 чанк по каждой оси).
            const std::int32_t stepX = (desiredOrigin.x == updatedOrigin.x) ? 0
                : ((desiredOrigin.x > updatedOrigin.x) ? 1 : -1);
            const std::int32_t stepY = (desiredOrigin.y == updatedOrigin.y) ? 0
                : ((desiredOrigin.y > updatedOrigin.y) ? 1 : -1);
                
            if (stepX == 0 && stepY == 0) {
                return;
            }

            const core::spatial::ChunkCoord nextOrigin{updatedOrigin.x + stepX,
                                                       updatedOrigin.y + stepY};
            buildWindowDelta(updatedOrigin, nextOrigin, mUnloadScratch, mLoadScratch);

            // Если превышен лимит на кадр, откладываем сдвиг до следующего обновления.
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
                ++mLoadedChunkCount;
#if !defined(NDEBUG) || defined(RETRIBUTIO_PROFILE)
                ++mDebugFrameLoads;
#endif
            }
        }

        void render(core::ecs::World&, sf::RenderWindow&) override {}

#if !defined(NDEBUG) || defined(RETRIBUTIO_PROFILE)
        [[nodiscard]] std::uint32_t loadedChunkCount() const noexcept { return mLoadedChunkCount; }
        [[nodiscard]] std::uint32_t windowTotalChunks() const noexcept { return mWindowTotalChunks; }
        [[nodiscard]] bool isInitialLoadComplete() const noexcept {
            return mInitPhase == InitPhase::Complete;
        }
        [[nodiscard]] InitPhase initPhase() const noexcept { return mInitPhase; }
        [[nodiscard]] std::uint32_t debugFrameLoads() const noexcept { return mDebugFrameLoads; }
        [[nodiscard]] std::uint32_t debugFrameUnloads() const noexcept { return mDebugFrameUnloads; }
        [[nodiscard]] core::spatial::ChunkCoord debugDesiredOrigin() const noexcept {
            return mDebugDesiredOrigin;
        }
        [[nodiscard]] std::size_t debugMaxEntitiesPerChunk() const noexcept {
            return mMaxEntitiesPerChunk;
        }

        /// Количество чанков, для которых были заспавнены сущности (прогресс Phase 2).
        /// loadedChunkCount отслеживает Phase 1 (состояние->Loaded); этот метод - Phase 2.
        /// Только после того как оба счетчика == windowTotalChunks загрузка реально завершена.
        [[nodiscard]] std::uint32_t debugSpawnedChunkCount() const noexcept {
            // Явный case для каждого элемента (для совместимости с MSVC C4061).
            switch (mInitPhase) {
            case InitPhase::Complete:
                return mWindowTotalChunks;
            case InitPhase::SpawningEntities:
                return mInitCursor; // Курсор в Phase 2 = кол-во уже созданных чанков
            case InitPhase::NotStarted:
            case InitPhase::SettingLoaded:
                return 0u; // Сущности еще не создавались
            }
            return 0u; // Недостижимо, подавляет предупреждения на других компиляторах
        }
#endif

      private:

        [[nodiscard]] core::spatial::ChunkCoord computeFocusChunk(
            const float x, const float y) const noexcept {
            const float invChunk = 1.0f / static_cast<float>(mChunkSizeWorld);
            const std::int32_t cx = static_cast<std::int32_t>(std::floor(x * invChunk));
            const std::int32_t cy = static_cast<std::int32_t>(std::floor(y * invChunk));
            return core::spatial::ChunkCoord{cx, cy};
        }

        [[nodiscard]] std::size_t membershipSlot(
            const core::spatial::ChunkCoord coord) const noexcept {
            const std::int32_t wx = core::spatial::detail::euclideanMod(coord.x, mWindowWidth);
            const std::int32_t wy = core::spatial::detail::euclideanMod(coord.y, mWindowHeight);
            return static_cast<std::size_t>(wy) * static_cast<std::size_t>(mWindowWidth) +
                   static_cast<std::size_t>(wx);
        }

        [[nodiscard]] core::spatial::ChunkCoord
        computeDesiredOrigin(const core::spatial::ChunkCoord currentOrigin,
                             const core::spatial::ChunkCoord focus,
                             const std::int32_t width,
                             const std::int32_t height) const noexcept {
            core::spatial::ChunkCoord desiredOrigin = currentOrigin;
            
            // Внутренняя (гистерезисная) зона окна, где движение игрока не вызывает сдвиг.
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

        [[nodiscard]] core::spatial::ChunkCoord cursorToChunkCoord(
            const std::uint32_t cursor,
            const core::spatial::ChunkCoord origin) const noexcept {
            const auto width = static_cast<std::uint32_t>(mWindowWidth);
            const std::int32_t localX = static_cast<std::int32_t>(cursor % width);
            const std::int32_t localY = static_cast<std::int32_t>(cursor / width);
            return core::spatial::ChunkCoord{origin.x + localX, origin.y + localY};
        }

        void beginInitialLoad(core::ecs::World& world) {
            if (mContentProvider == nullptr) {
                mContentProvider = &mEmptyProvider;
            }
            if (mDeferredDestroyReserve > 0) {
                world.reserveDeferredDestroyQueue(mDeferredDestroyReserve);
            }
            mInitCursor = 0;
            mInitPhase = InitPhase::SettingLoaded;
            mLoadedChunkCount = 0;
        }

        void continueInitialLoad(core::ecs::World& world) {
            auto& index = mSpatialSystem->index();
            const core::spatial::ChunkCoord origin = index.windowOrigin();
            const std::uint32_t total = mWindowTotalChunks;

            switch (mInitPhase) {

            case InitPhase::SettingLoaded: {
                std::uint32_t processed = 0;
                while (mInitCursor < total && processed < mMaxLoadsPerFrame) {
                    const core::spatial::ChunkCoord coord = cursorToChunkCoord(mInitCursor, origin);
                    if (!index.setChunkState(coord, core::spatial::ResidencyState::Loaded)) {
                        LOG_PANIC(core::log::cat::ECS,
                                  "SpatialStreamingSystem: failed to set chunk Loaded ({}, {})"
                                  " during initial load phase 1", coord.x, coord.y);
                    }
                    ++processed;
                    ++mInitCursor;
                    ++mLoadedChunkCount;
#if !defined(NDEBUG) || defined(RETRIBUTIO_PROFILE)
                    ++mDebugFrameLoads;
#endif
                }
                if (mInitCursor >= total) {
                    LOG_INFO(core::log::cat::ECS,
                             "SpatialStreamingSystem: initial load phase 1 complete "
                             "({} chunks set to Loaded, {} frames)",
                             total,
                             (mMaxLoadsPerFrame > 0u)
                                 ? ((total + mMaxLoadsPerFrame - 1u) / mMaxLoadsPerFrame)
                                 : 0u);
                    mInitPhase = InitPhase::SpawningEntities;
                    mInitCursor = 0;
                }
                break;
            }

            case InitPhase::SpawningEntities: {
                std::uint32_t processed = 0;
                while (mInitCursor < total && processed < mMaxInitSpawnChunksPerFrame) {
                    const core::spatial::ChunkCoord coord = cursorToChunkCoord(mInitCursor, origin);
                    
                    // Загружаем сущности в уже "Loaded" чанк.
                    loadChunk(world, coord);
                    
#if !defined(NDEBUG) || defined(RETRIBUTIO_PROFILE)
                    ++mDebugFrameLoads;
#endif
                    ++processed;
                    ++mInitCursor;
                }
                if (mInitCursor >= total) {
                    mInitPhase = InitPhase::Complete;
                    
                    const std::uint32_t phase1Frames =
                        (mMaxLoadsPerFrame > 0u)
                            ? ((total + mMaxLoadsPerFrame - 1u) / mMaxLoadsPerFrame)
                            : 0u;
                    const std::uint32_t phase2Frames =
                        (mMaxInitSpawnChunksPerFrame > 0u)
                            ? ((total + mMaxInitSpawnChunksPerFrame - 1u) /
                               mMaxInitSpawnChunksPerFrame)
                            : 0u;
                            
                    LOG_INFO(core::log::cat::ECS,
                             "SpatialStreamingSystem: initial load complete "
                             "({} chunks, phase1={} frames, phase2={} frames, total={} frames, "
                             "spawnBudget={} chunks/frame)",
                             total, phase1Frames, phase2Frames,
                             phase1Frames + phase2Frames, mMaxInitSpawnChunksPerFrame);
                }
                break;
            }

            case InitPhase::Complete:
            case InitPhase::NotStarted:
                break;
            }
        }

        void loadChunk(core::ecs::World& world, const core::spatial::ChunkCoord coord) {
            if (mContentProvider == nullptr) {
                mContentProvider = &mEmptyProvider;
            }
            if (mMaxEntitiesPerChunk == 0u) {
                return;
            }

            auto& index = mSpatialSystem->index();
            const auto st = index.chunkState(coord);
            if (st != core::spatial::ResidencyState::Loaded) {
                LOG_PANIC(core::log::cat::ECS,
                          "SpatialStreamingSystem: loadChunk() called for non-Loaded chunk "
                          "({}, {}) state={}",
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
                          "SpatialStreamingSystem: slot out of range for chunk ({}, {}) "
                          "slot={} windowSize={}",
                          coord.x, coord.y, slot, mChunkEntityCounts.size());
            }
            if (mChunkEntityStride == 0u || mChunkEntities.empty()) {
                LOG_PANIC(core::log::cat::ECS,
                          "SpatialStreamingSystem: membership pool not initialized (stride=0)");
            }
            if (mChunkEntityCounts[slot] != 0u) {
                LOG_PANIC(core::log::cat::ECS,
                          "SpatialStreamingSystem: double-load detected for chunk ({}, {}) "
                          "slot={} count={}",
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
                const bool finite = std::isfinite(desc.localPos.x) &&
                                    std::isfinite(desc.localPos.y) &&
                                    std::isfinite(desc.scale.x) &&
                                    std::isfinite(desc.scale.y) &&
                                    std::isfinite(desc.origin.x) &&
                                    std::isfinite(desc.origin.y) &&
                                    std::isfinite(desc.zOrder);
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
                mChunkEntities[base + i] = entity;
            }
            mChunkEntityCounts[slot] = static_cast<std::uint32_t>(count);
        }

        void unloadChunk(core::ecs::World& world, const core::spatial::ChunkCoord coord) {
            auto& index = mSpatialSystem->index();
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
                    
                    // Безопасность (streamed-out safety): тег предотвращает повторную регистрацию.
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

            const bool unloaded = index.setChunkState(coord,
                                                      core::spatial::ResidencyState::Unloaded);
            if (!unloaded) {
                LOG_PANIC(core::log::cat::ECS,
                          "SpatialStreamingSystem: failed to unload chunk ({}, {})",
                          coord.x, coord.y);
            }
            if (mLoadedChunkCount > 0u) {
                --mLoadedChunkCount;
            }
#if !defined(NDEBUG) || defined(RETRIBUTIO_PROFILE)
            ++mDebugFrameUnloads;
#endif
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

        InitPhase mInitPhase = InitPhase::NotStarted;
        std::uint32_t mInitCursor = 0;
        std::uint32_t mMaxInitSpawnChunksPerFrame = 0;
        std::uint32_t mLoadedChunkCount = 0;
        std::uint32_t mWindowTotalChunks = 0;

        std::vector<core::spatial::ChunkCoord> mUnloadScratch{};
        std::vector<core::spatial::ChunkCoord> mLoadScratch{};
        std::vector<core::ecs::Entity> mDestroyScratch{};
        std::vector<streaming::ChunkEntityDesc> mSpawnBuffer{};
        std::size_t mChunkEntityStride = 0;
        std::vector<std::uint32_t> mChunkEntityCounts{};
        std::vector<core::ecs::Entity> mChunkEntities{};

#if !defined(NDEBUG) || defined(RETRIBUTIO_PROFILE)
        std::uint32_t mDebugFrameLoads = 0;
        std::uint32_t mDebugFrameUnloads = 0;
        core::spatial::ChunkCoord mDebugDesiredOrigin{0, 0};
#endif
    };

} // namespace game::atrapacielos::ecs