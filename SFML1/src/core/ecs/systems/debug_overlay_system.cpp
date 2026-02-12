#include "pch.h"

#include "core/ecs/systems/debug_overlay_system.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "core/config/properties/debug_overlay_runtime_properties.h"
#include "core/config/properties/text_properties.h"
#include "core/ecs/components/spatial_dirty_tag.h"
#include "core/ecs/components/spatial_id_component.h"
#include "core/ecs/components/spatial_streamed_out_tag.h"
#include "core/ecs/components/sprite_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/world.h"
#include "core/log/log_macros.h"
#include "core/time/time_service.h"
#include "core/utils/format/append_numbers.h"

// RenderSystem нужен только для Debug/Profile (статистика рендера).
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
#include "core/ecs/systems/render_system.h"
#include "core/ecs/systems/spatial_index_system.h"
#endif

#if defined(SFML1_PROFILE)
namespace {
    [[nodiscard]] std::uint64_t emaPow2(std::uint64_t prev, std::uint64_t sample,
                                        const std::uint8_t shift) noexcept {
        // EMA без float: alpha = 1 / (2^shift)
        // shift==0 => без сглаживания.
        if (shift == 0) {
            return sample;
        }
        if (prev == 0) {
            return sample;
        }

        const std::uint64_t denom = (1ull << shift); // shift гарантированно < 64
        const std::uint64_t mask = (denom - 1);      // denom == 2^shift
        const std::uint64_t half = (denom >> 1);      // округление

        const auto roundDivPow2 = [shift, mask, half](const std::uint64_t diff) noexcept {
            const std::uint64_t q = (diff >> shift);
            const std::uint64_t r = (diff & mask);
            return q + ((r >= half) ? 1ull : 0ull);
        };

        if (sample >= prev) {
            return prev + roundDivPow2(sample - prev);
        }

        const std::uint64_t delta = roundDivPow2(prev - sample);
        return (prev > delta) ? (prev - delta) : 0;
    }
} // namespace
#endif

namespace {

    /**
     * @brief Bounded appender для debug overlay text buffers.
     *
     * Предотвращает неограниченный рост буфера: при превышении hard cap
     * дописывает видимый маркер "[TRUNCATED]" и прекращает все дальнейшие записи.
     *
     * Два режима:
     *  - appendAtomic(): all-or-nothing (для структурированных key=value строк).
     *  - appendClamped(): допускает частичную запись (для пользовательского extra text).
     *
     * ВАЖНО:
     *  - maxBeforeMarkerBytes = hardCapBytes - truncMarker.size() (считается вызывающим).
     *  - Контракт: hardCapBytes > truncMarker.size() (гарантируется static_assert в .h).
     */
    struct CappedAppender final {
        std::string& dst;
        const std::size_t maxBeforeMarkerBytes;
        const std::string_view truncMarker;
        bool& truncated;

        void appendAtomic(const std::string_view s) {
            if (!ensure(s.size())) {
                return;
            }
            dst.append(s);
        }

        void pushBackAtomic(const char c) {
            if (!ensure(1u)) {
                return;
            }
            dst.push_back(c);
        }

        void appendU64Atomic(const std::uint64_t value) {
            // uint64 max = 20 десятичных цифр.
            if (!ensure(20u)) {
                return;
            }
            core::utils::format::appendU64(dst, value);
        }

#if defined(SFML1_PROFILE)
        void appendAdaptiveTimingFromUsAtomic(const std::uint64_t us) {
            // Консервативно: uint64 + suffix ("us"/"ms") + decimal part.
            if (!ensure(32u)) {
                return;
            }
            core::utils::format::appendAdaptiveTimingFromUs(dst, us);
        }
#endif

        /// Допускает частичную запись: дописывает сколько влезает, затем truncation.
        void appendClamped(const std::string_view s) {
            if (truncated) {
                return;
            }

            const std::size_t used = dst.size();
            const std::size_t remaining =
                (used < maxBeforeMarkerBytes) ? (maxBeforeMarkerBytes - used) : 0u;

            if (s.size() <= remaining) {
                dst.append(s);
                return;
            }

            if (remaining > 0u) {
                dst.append(s.data(), remaining);
            }
            truncateNow();
        }

      private:
        [[nodiscard]] bool ensure(const std::size_t bytesToAppend) {
            if (truncated) {
                return false;
            }
            if (dst.size() + bytesToAppend > maxBeforeMarkerBytes) {
                truncateNow();
                return false;
            }
            return true;
        }

        void truncateNow() {
            if (truncated) {
                return;
            }
            truncated = true;
            dst.append(truncMarker);
        }
    };

} // namespace

namespace core::ecs {

    void DebugOverlaySystem::bind(core::time::TimeService& timeService, const sf::Font& font) {
        mTime = &timeService;
        mFpsText.emplace(font);

        mTextBuffer.clear();
        mTextBuffer.reserve(kTextHardCapBytes);

        mExtraTextBuffer.clear();
        mExtraTextBuffer.reserve(kExtraTextHardCapBytes);

        mTextTruncated = false;
        mTextTruncLogged = false;
        mExtraTextTruncated = false;

        mRenderClock.restart();
        mAccumulatedTime = mUpdateInterval; // чтобы первая отрисовка сразу обновила текст
    }

    void DebugOverlaySystem::applyRuntimeProperties(
        const core::config::properties::DebugOverlayRuntimeProperties& props) noexcept {
        // Контракт: loader валидирует/клампит. Здесь trust-on-read.
        assert(props.updateIntervalMs <=
                   core::config::properties::DebugOverlayRuntimeProperties::kMaxUpdateIntervalMs &&
               "updateIntervalMs violates contract (must be clamped in loader)");
        assert(props.smoothingShift <=
                   core::config::properties::DebugOverlayRuntimeProperties::kMaxSmoothingShift &&
               "smoothingShift violates contract (must be clamped in loader)");

        // 0 ms => обновлять каждый кадр.
        if (props.updateIntervalMs == 0) {
            mUpdateInterval = sf::Time::Zero;
        } else {
            mUpdateInterval = sf::milliseconds(static_cast<int>(props.updateIntervalMs));
        }

        mSmoothingShift = props.smoothingShift;

        // При изменении режима сбрасываем сглаживание, чтобы не было "хвоста".
#if defined(SFML1_PROFILE)
        mSmoothedCpuTotalUs = 0;
        mSmoothedRSGatherUs = 0;
        mSmoothedRSSortUs = 0;
        mSmoothedRSBuildUs = 0;
        mSmoothedRSDrawUs = 0;
#endif

        // Truncation мог случиться при старых настройках — разрешаем лог заново.
        mTextTruncated = false;
        mTextTruncLogged = false;
        mExtraTextTruncated = false;

        // И таймер тоже, чтобы изменение применилось сразу.
        mRenderClock.restart();
        mAccumulatedTime = mUpdateInterval;
    }

    void
    DebugOverlaySystem::applyTextProperties(const core::config::properties::TextProperties& props) {
        if (!mFpsText) {
            return;
        }

        mFpsText->setPosition(props.position);
        mFpsText->setCharacterSize(static_cast<unsigned int>(props.characterSize));
        mFpsText->setFillColor(props.color);
    }

    void DebugOverlaySystem::update(World& world, float dt) {
        // Render-only система.
        // Важно: update() может вызываться 0..N раз за кадр (фиксированный timestep),
        // поэтому любые "per-frame" метрики/строки обновляем в prepareFrame() ровно один раз.
        (void) world;
        (void) dt;
    }

    void DebugOverlaySystem::prepareFrame(World& world) {
        if (!mEnabled || !mFpsText) {
            return;
        }

        const sf::Time dt = mRenderClock.restart();
        mAccumulatedTime += dt;
        if (mUpdateInterval != sf::Time::Zero && mAccumulatedTime < mUpdateInterval) {
            return;
        }
        mAccumulatedTime = sf::Time::Zero;

        mTextBuffer.clear();
        mTextTruncated = false;

        CappedAppender out{mTextBuffer,
                           kTextHardCapBytes - kTruncMarker.size(),
                           kTruncMarker,
                           mTextTruncated};

        // ----------------------------------------------------------------------------------------
        // FPS — показываем ВСЕГДА (Debug/Profile/Release)
        // ----------------------------------------------------------------------------------------
        out.appendAtomic("fps=");
        if (mTime) {
            const int fpsRounded = static_cast<int>(mTime->getSmoothedFps());
            out.appendU64Atomic(static_cast<std::uint64_t>(fpsRounded));
        } else {
            out.appendAtomic("?");
        }

        // ----------------------------------------------------------------------------------------
        // Counts — ecsAlive всегда, spatialActive дописывается в Debug/Profile (если есть).
        //
        // Семантически разные registry: ecsAlive = все entity в EnTT,
        // spatialActive = зарегистрированные в SpatialIndex.
        // Расхождение диагностически ценно (streamed-out, сервисные entity, теги).
        // ----------------------------------------------------------------------------------------
        out.appendAtomic("\nCounts: ecsAlive=");
        out.appendU64Atomic(static_cast<std::uint64_t>(world.aliveEntityCount()));

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        // spatialActive дописываем на ту же строку "Counts:" до блока render stats.
        if (mSpatialIndexSystem) {
            const auto& idx = mSpatialIndexSystem->index();
            out.appendAtomic(" spatialActive=");
            out.appendU64Atomic(static_cast<std::uint64_t>(idx.activeEntityCount()));
        }

        // ----------------------------------------------------------------------------------------
        // Статистика рендера — только Debug/Profile.
        // ----------------------------------------------------------------------------------------
        if (mRenderSystem) {
            const auto& stats = mRenderSystem->getLastFrameStatsRef();

            // Каноническое "сколько реально отрисовано спрайтов" = stats.spriteCount.
            // Никаких дублей "drawn" в других блоках.
            out.appendAtomic("\nRender: sprites=");
            out.appendU64Atomic(static_cast<std::uint64_t>(stats.spriteCount));
            out.appendAtomic(" vertices=");
            out.appendU64Atomic(static_cast<std::uint64_t>(stats.vertexCount));
            out.appendAtomic(" drawCalls=");
            out.appendU64Atomic(static_cast<std::uint64_t>(stats.batchDrawCalls));

            out.appendAtomic("\nTextures: switches=");
            out.appendU64Atomic(static_cast<std::uint64_t>(stats.textureSwitches));
            out.appendAtomic(" unique=");
            out.appendU64Atomic(static_cast<std::uint64_t>(stats.uniqueTexturePointers));
            out.appendAtomic(" cacheSize=");
            out.appendU64Atomic(static_cast<std::uint64_t>(stats.textureCacheSize));
            out.appendAtomic(" residentFetches=");
            out.appendU64Atomic(static_cast<std::uint64_t>(stats.resourceLookupsThisFrame));

            // Culling: key=value, без "A/B/C" формата.
            // candidates = сколько кандидатов дошло до gather (без mapNull/missingComponents).
            // culled = сколько отсекли fine-cull/invalid data.
            out.appendAtomic("\nCulling: candidates=");
            out.appendU64Atomic(static_cast<std::uint64_t>(stats.totalSpriteCount));
            out.appendAtomic(" culled=");
            out.appendU64Atomic(static_cast<std::uint64_t>(stats.culledSpriteCount));

            // RenderFilter: причины "почему не дошло до draw".
            // ВАЖНО: не дублируем drawn/sprites.
            out.appendAtomic("\nRenderFilter: mapNull=");
            out.appendU64Atomic(static_cast<std::uint64_t>(stats.renderMapNull));
            out.appendAtomic(" missingComponents=");
            out.appendU64Atomic(static_cast<std::uint64_t>(stats.renderMissingComponents));
            out.appendAtomic(" fineCullFail=");
            out.appendU64Atomic(static_cast<std::uint64_t>(stats.renderFineCullFail));

#if defined(SFML1_PROFILE)
            // CPU: только totalMs (drawMs — в RenderCPU breakdown, без дублирования).
            mSmoothedCpuTotalUs = emaPow2(mSmoothedCpuTotalUs, stats.cpuTotalUs, mSmoothingShift);

            out.appendAtomic("\nCPU: total=");
            out.appendAdaptiveTimingFromUsAtomic(mSmoothedCpuTotalUs);

            // Разбивка RenderSystem (gather/sort/build/draw).
            mSmoothedRSGatherUs = emaPow2(mSmoothedRSGatherUs, stats.cpuGatherUs, mSmoothingShift);
            mSmoothedRSSortUs = emaPow2(mSmoothedRSSortUs, stats.cpuSortUs, mSmoothingShift);
            mSmoothedRSBuildUs = emaPow2(mSmoothedRSBuildUs, stats.cpuBuildUs, mSmoothingShift);
            mSmoothedRSDrawUs = emaPow2(mSmoothedRSDrawUs, stats.cpuDrawUs, mSmoothingShift);

            out.appendAtomic("\nRenderCPU: gather=");
            out.appendAdaptiveTimingFromUsAtomic(mSmoothedRSGatherUs);
            out.appendAtomic(" sort=");
            out.appendAdaptiveTimingFromUsAtomic(mSmoothedRSSortUs);
            out.appendAtomic(" build=");
            out.appendAdaptiveTimingFromUsAtomic(mSmoothedRSBuildUs);
            out.appendAtomic(" draw=");
            out.appendAdaptiveTimingFromUsAtomic(mSmoothedRSDrawUs);
#endif
        }

        // ----------------------------------------------------------------------------------------
        // Spatial index метрики — только Debug/Profile.
        // Grid geometry/structure отделён от Counts для разгрузки строки.
        // ----------------------------------------------------------------------------------------
        if (mSpatialIndexSystem) {
            const auto& idx = mSpatialIndexSystem->index();

            out.appendAtomic("\nSpatialGrid: window=");
            out.appendU64Atomic(static_cast<std::uint64_t>(idx.windowWidth()));
            out.pushBackAtomic('x');
            out.appendU64Atomic(static_cast<std::uint64_t>(idx.windowHeight()));

            out.appendAtomic(" dupInsertDetected=");
            out.appendU64Atomic(
                static_cast<std::uint64_t>(idx.debugDupInsertDetected()));

            // Канонический блок query counters: только из SpatialIndex.
            const auto& q = idx.debugLastQueryStatsRef();
            out.appendAtomic("\nSpatialQuery: entriesScanned=");
            out.appendU64Atomic(static_cast<std::uint64_t>(q.entriesScanned));
            out.appendAtomic(" unique=");
            out.appendU64Atomic(static_cast<std::uint64_t>(q.uniqueAdded));
            out.appendAtomic(" dupHits=");
            out.appendU64Atomic(static_cast<std::uint64_t>(q.dupHits));
            out.appendAtomic(" truncated=");
            out.appendU64Atomic(static_cast<std::uint64_t>(q.outTruncated));

            // chunksLoadedVisited - загруженные чанки, посещённые ПОСЛЕДНИМ query,
            //                       не глобально загруженные чанки.
            out.appendAtomic("\nSpatialVisit: chunksLoadedVisited=");
            out.appendU64Atomic(static_cast<std::uint64_t>(q.chunksLoadedVisited));
            out.appendAtomic(" chunks=");
            out.appendU64Atomic(static_cast<std::uint64_t>(q.chunksVisited));
            out.appendAtomic(" skippedNonLoaded=");
            out.appendU64Atomic(static_cast<std::uint64_t>(q.chunksSkippedNonLoaded));
            out.appendAtomic(" cells=");
            out.appendU64Atomic(static_cast<std::uint64_t>(q.cellVisits));
            out.appendAtomic(" overflowCells=");
            out.appendU64Atomic(static_cast<std::uint64_t>(q.overflowCellVisits));

            // Storages: sizes (диагностика компонентного состава).
            out.appendAtomic("\nStorages: transform=");
            out.appendU64Atomic(
                static_cast<std::uint64_t>(world.debugStorageSize<TransformComponent>()));
            out.appendAtomic(" sprite=");
            out.appendU64Atomic(
                static_cast<std::uint64_t>(world.debugStorageSize<SpriteComponent>()));
            out.appendAtomic(" spatialId=");
            out.appendU64Atomic(
                static_cast<std::uint64_t>(world.debugStorageSize<SpatialIdComponent>()));
            out.appendAtomic(" streamedOut=");
            out.appendU64Atomic(
                static_cast<std::uint64_t>(world.debugStorageSize<SpatialStreamedOutTag>()));
            out.appendAtomic(" dirty=");
            out.appendU64Atomic(
                static_cast<std::uint64_t>(world.debugStorageSize<SpatialDirtyTag>()));
        }
#endif // !defined(NDEBUG) || defined(SFML1_PROFILE)

        if (!mExtraTextBuffer.empty()) {
            out.appendAtomic("\n");
            // extra может быть большим: допускаем частичную допись.
            out.appendClamped(mExtraTextBuffer);
        }

        if (mTextTruncated && !mTextTruncLogged) {
            mTextTruncLogged = true;
            LOG_WARN(core::log::cat::ECS,
                     "DebugOverlay: text buffer truncated (cap={} bytes). "
                     "Reduce overlay lines or increase kTextHardCapBytes.",
                     kTextHardCapBytes);
        }

        mFpsText->setString(mTextBuffer);
    }

    void DebugOverlaySystem::prepareFrame(World& world, const std::string_view extraText) {
        clearExtraText();
        if (!extraText.empty()) {
            appendExtraLine(extraText);
        }
        prepareFrame(world);
    }

    void DebugOverlaySystem::clearExtraText() noexcept {
        mExtraTextBuffer.clear();
        mExtraTextTruncated = false;
    }

    void DebugOverlaySystem::appendExtraLine(const std::string_view line) {
        if (line.empty() || mExtraTextTruncated) {
            return;
        }

        CappedAppender out{mExtraTextBuffer,
                           kExtraTextHardCapBytes - kTruncMarker.size(),
                           kTruncMarker,
                           mExtraTextTruncated};

        if (!mExtraTextBuffer.empty()) {
            out.appendAtomic("\n");
        }

        // extra строки — могут быть длинными: дописываем частично.
        out.appendClamped(line);
    }

    void DebugOverlaySystem::draw(sf::RenderWindow& window) const {
        if (!mEnabled || !mFpsText) {
            return;
        }
        window.draw(*mFpsText);
    }

} // namespace core::ecs