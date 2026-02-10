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
        const std::uint64_t half = (denom >> 1);     // округление

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

    // Жёсткие бюджеты на строки оверлея:
    // - без realloc в hot-path,
    // - без fragile assert на capacity при росте extra-строк.
    constexpr std::size_t kTextReserveBytes = 2048;
    constexpr std::size_t kExtraTextReserveBytes = 2048;

} // namespace

namespace core::ecs {

    void DebugOverlaySystem::bind(core::time::TimeService& timeService, const sf::Font& font) {
        mTime = &timeService;
        mFpsText.emplace(font);

        mTextBuffer.clear();
        mTextBuffer.reserve(kTextReserveBytes);

        mExtraTextBuffer.clear();
        mExtraTextBuffer.reserve(kExtraTextReserveBytes);

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
        mSmoothedCpuDrawUs = 0;
        mSmoothedRSGatherUs = 0;
        mSmoothedRSSortUs = 0;
        mSmoothedRSBuildUs = 0;
        mSmoothedRSDrawUs = 0;
#endif
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

        // ----------------------------------------------------------------------------------------
        // FPS — показываем ВСЕГДА (Debug/Profile/Release)
        // ----------------------------------------------------------------------------------------
        mTextBuffer.append("fps=");
        if (mTime) {
            const int fpsRounded = static_cast<int>(mTime->getSmoothedFps());
            core::utils::format::appendU64(mTextBuffer, static_cast<std::uint64_t>(fpsRounded));
        } else {
            mTextBuffer.append("?");
        }

        // ----------------------------------------------------------------------------------------
        // Entity count — показываем ВСЕГДА (O(1) чтение)
        // ----------------------------------------------------------------------------------------
        mTextBuffer.append("\nECS: alive=");
        core::utils::format::appendU64(mTextBuffer,
                                       static_cast<std::uint64_t>(world.aliveEntityCount()));

        // ----------------------------------------------------------------------------------------
        // Статистика рендера — только Debug/Profile.
        // ----------------------------------------------------------------------------------------
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        if (mRenderSystem) {
            const auto& stats = mRenderSystem->getLastFrameStatsRef();

            // Каноническое "сколько реально отрисовано спрайтов" = stats.spriteCount.
            // Никаких дублей "drawn" в других блоках.
            mTextBuffer.append("\nRender: sprites=");
            core::utils::format::appendU64(mTextBuffer,
                                           static_cast<std::uint64_t>(stats.spriteCount));
            mTextBuffer.append(" vertices=");
            core::utils::format::appendU64(mTextBuffer,
                                           static_cast<std::uint64_t>(stats.vertexCount));
            mTextBuffer.append(" drawCalls=");
            core::utils::format::appendU64(mTextBuffer,
                                           static_cast<std::uint64_t>(stats.batchDrawCalls));

            mTextBuffer.append("\nTextures: switches=");
            core::utils::format::appendU64(mTextBuffer,
                                           static_cast<std::uint64_t>(stats.textureSwitches));
            mTextBuffer.append(" unique=");
            core::utils::format::appendU64(mTextBuffer,
                                           static_cast<std::uint64_t>(stats.uniqueTexturePointers));
            mTextBuffer.append(" cacheSize=");
            core::utils::format::appendU64(mTextBuffer,
                                           static_cast<std::uint64_t>(stats.textureCacheSize));
            mTextBuffer.append(" residentFetches=");
            core::utils::format::appendU64(
                mTextBuffer, static_cast<std::uint64_t>(stats.resourceLookupsThisFrame));

            // Culling: key=value, без "A/B/C" формата.
            // candidates = сколько кандидатов дошло до gather (без mapNull/missingComponents).
            // culled = сколько отсекли fine-cull/invalid data.
            mTextBuffer.append("\nCulling: drawn=");
            core::utils::format::appendU64(mTextBuffer,
                                           static_cast<std::uint64_t>(stats.spriteCount));
            mTextBuffer.append(" candidates=");
            core::utils::format::appendU64(mTextBuffer,
                                           static_cast<std::uint64_t>(stats.totalSpriteCount));
            mTextBuffer.append(" culled=");
            core::utils::format::appendU64(mTextBuffer,
                                           static_cast<std::uint64_t>(stats.culledSpriteCount));

            // RenderFilter: причины "почему не дошло до draw".
            // ВАЖНО: не дублируем drawn/sprites.
            mTextBuffer.append("\nRenderFilter: mapNull=");
            core::utils::format::appendU64(mTextBuffer,
                                           static_cast<std::uint64_t>(stats.renderMapNull));
            mTextBuffer.append(" missingComponents=");
            core::utils::format::appendU64(
                mTextBuffer, static_cast<std::uint64_t>(stats.renderMissingComponents));
            mTextBuffer.append(" fineCullFail=");
            core::utils::format::appendU64(mTextBuffer,
                                           static_cast<std::uint64_t>(stats.renderFineCullFail));

#if defined(SFML1_PROFILE)
            // Тайминги CPU — только Profile.
            mSmoothedCpuTotalUs = emaPow2(mSmoothedCpuTotalUs, stats.cpuTotalUs, mSmoothingShift);
            mSmoothedCpuDrawUs = emaPow2(mSmoothedCpuDrawUs, stats.cpuDrawUs, mSmoothingShift);

            mTextBuffer.append("\nCPU: totalMs=");
            core::utils::format::appendMs1DecimalFromUs(mTextBuffer, mSmoothedCpuTotalUs);
            mTextBuffer.append(" drawMs=");
            core::utils::format::appendMs1DecimalFromUs(mTextBuffer, mSmoothedCpuDrawUs);

            // Разбивка RenderSystem (gather/sort/build/draw).
            mSmoothedRSGatherUs = emaPow2(mSmoothedRSGatherUs, stats.cpuGatherUs, mSmoothingShift);
            mSmoothedRSSortUs = emaPow2(mSmoothedRSSortUs, stats.cpuSortUs, mSmoothingShift);
            mSmoothedRSBuildUs = emaPow2(mSmoothedRSBuildUs, stats.cpuBuildUs, mSmoothingShift);
            mSmoothedRSDrawUs = emaPow2(mSmoothedRSDrawUs, stats.cpuDrawUs, mSmoothingShift);

            mTextBuffer.append("\nRenderCPU: gatherMs=");
            core::utils::format::appendMs1DecimalFromUs(mTextBuffer, mSmoothedRSGatherUs);
            mTextBuffer.append(" sortMs=");
            core::utils::format::appendMs1DecimalFromUs(mTextBuffer, mSmoothedRSSortUs);
            mTextBuffer.append(" buildMs=");
            core::utils::format::appendMs1DecimalFromUs(mTextBuffer, mSmoothedRSBuildUs);
            mTextBuffer.append(" drawMs=");
            core::utils::format::appendMs1DecimalFromUs(mTextBuffer, mSmoothedRSDrawUs);
#endif
        }

        // ----------------------------------------------------------------------------------------
        // Spatial index метрики — только Debug/Profile.
        // SpatialActive != ECSAlive: count зарегистрированных в индексе entity.
        // ----------------------------------------------------------------------------------------
        if (mSpatialIndexSystem) {
            const auto& idx = mSpatialIndexSystem->index();

            mTextBuffer.append("\nSpatial: active=");
            core::utils::format::appendU64(mTextBuffer,
                                           static_cast<std::uint64_t>(idx.activeEntityCount()));

            mTextBuffer.append(" window=");
            core::utils::format::appendU64(mTextBuffer,
                                           static_cast<std::uint64_t>(idx.windowWidth()));
            mTextBuffer.push_back('x');
            core::utils::format::appendU64(mTextBuffer,
                                           static_cast<std::uint64_t>(idx.windowHeight()));

            mTextBuffer.append(" dupInsertDetected=");
            core::utils::format::appendU64(
                mTextBuffer, static_cast<std::uint64_t>(idx.debugDupInsertDetected()));

            // Канонический блок query counters: только из SpatialIndex.
            const auto& q = idx.debugLastQueryStatsRef();
            mTextBuffer.append("\nSpatialQuery: entriesScanned=");
            core::utils::format::appendU64(mTextBuffer,
                                           static_cast<std::uint64_t>(q.entriesScanned));
            mTextBuffer.append(" unique=");
            core::utils::format::appendU64(mTextBuffer, static_cast<std::uint64_t>(q.uniqueAdded));
            mTextBuffer.append(" dupHits=");
            core::utils::format::appendU64(mTextBuffer, static_cast<std::uint64_t>(q.dupHits));
            mTextBuffer.append(" cells=");
            core::utils::format::appendU64(mTextBuffer, static_cast<std::uint64_t>(q.cellVisits));
            mTextBuffer.append(" ovCells=");
            core::utils::format::appendU64(mTextBuffer,
                                           static_cast<std::uint64_t>(q.overflowCellVisits));
            mTextBuffer.append(" chunksLoaded=");
            core::utils::format::appendU64(mTextBuffer,
                                           static_cast<std::uint64_t>(q.chunksLoadedVisited));
            mTextBuffer.append(" chunks=");
            core::utils::format::appendU64(mTextBuffer,
                                           static_cast<std::uint64_t>(q.chunksVisited));
            mTextBuffer.append(" skippedNonLoaded=");
            core::utils::format::appendU64(mTextBuffer,
                                           static_cast<std::uint64_t>(q.chunksSkippedNonLoaded));
            mTextBuffer.append(" truncated=");
            core::utils::format::appendU64(mTextBuffer, static_cast<std::uint64_t>(q.outTruncated));

            // Storages: sizes (диагностика компонентного состава).
            mTextBuffer.append("\nStorages: transform=");
            core::utils::format::appendU64(
                mTextBuffer,
                static_cast<std::uint64_t>(world.debugStorageSize<TransformComponent>()));
            mTextBuffer.append(" sprite=");
            core::utils::format::appendU64(
                mTextBuffer, static_cast<std::uint64_t>(world.debugStorageSize<SpriteComponent>()));
            mTextBuffer.append(" spatialId=");
            core::utils::format::appendU64(
                mTextBuffer,
                static_cast<std::uint64_t>(world.debugStorageSize<SpatialIdComponent>()));
            mTextBuffer.append(" streamedOut=");
            core::utils::format::appendU64(
                mTextBuffer,
                static_cast<std::uint64_t>(world.debugStorageSize<SpatialStreamedOutTag>()));
            mTextBuffer.append(" dirty=");
            core::utils::format::appendU64(
                mTextBuffer, static_cast<std::uint64_t>(world.debugStorageSize<SpatialDirtyTag>()));
        }
#endif // !defined(NDEBUG) || defined(SFML1_PROFILE)

        if (!mExtraTextBuffer.empty()) {
            mTextBuffer.push_back('\n');
            mTextBuffer.append(mExtraTextBuffer);
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
    }

    void DebugOverlaySystem::appendExtraLine(const std::string_view line) {
        if (line.empty()) {
            return;
        }

        // Hardening: никаких realloc/ассертов. Если не влезает — мягко обрезаем.
        // Важно: std::string не обязан realloc'ать, пока finalSize <= capacity().
        if (mExtraTextBuffer.size() >= mExtraTextBuffer.capacity()) {
            return;
        }

        if (!mExtraTextBuffer.empty()) {
            if (mExtraTextBuffer.size() >= mExtraTextBuffer.capacity()) {
                return;
            }
            mExtraTextBuffer.push_back('\n');
        }

        const std::size_t remaining = (mExtraTextBuffer.capacity() > mExtraTextBuffer.size())
                                          ? (mExtraTextBuffer.capacity() - mExtraTextBuffer.size())
                                          : 0u;
        if (remaining == 0u) {
            return;
        }

        const std::size_t toAppend = (line.size() <= remaining) ? line.size() : remaining;
        mExtraTextBuffer.append(line.data(), toAppend);
    }

    void DebugOverlaySystem::draw(sf::RenderWindow& window) const {
        if (!mEnabled || !mFpsText) {
            return;
        }
        window.draw(*mFpsText);
    }

} // namespace core::ecs
