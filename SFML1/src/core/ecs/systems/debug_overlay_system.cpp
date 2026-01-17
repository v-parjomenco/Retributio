#include "pch.h"

#include "core/ecs/systems/debug_overlay_system.h"

#include <array>
#include <charconv>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <system_error>

#include "core/config/properties/debug_overlay_runtime_properties.h"
#include "core/config/properties/text_properties.h"
#include "core/ecs/world.h"
#include "core/time/time_service.h"
#include "core/utils/format/append_numbers.h"

// RenderSystem нужен только для Debug/Profile (статистика рендера).
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
    #include "core/ecs/systems/render_system.h"
#endif

#if defined(SFML1_PROFILE)
namespace {
    [[nodiscard]] std::uint64_t emaPow2(std::uint64_t prev,
                                        std::uint64_t sample,
                                        const std::uint8_t shift) noexcept {
        // EMA без float: alpha = 1 / (2^shift)
        // shift==0 => без сглаживания.
        if (shift == 0) {
            return sample;
        }
        if (prev == 0) {
            return sample;
        }

        const std::uint64_t denom = (1ull << shift);   // shift гарантированно < 64
        const std::uint64_t mask  = (denom - 1);       // denom == 2^shift
        const std::uint64_t half  = (denom >> 1);      // округление

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

namespace core::ecs {

    void DebugOverlaySystem::bind(core::time::TimeService& timeService, const sf::Font& font) {
        mTime = &timeService;
        mFpsText.emplace(font);

        mTextBuffer.clear();
        mTextBuffer.reserve(512);
        mExtraTextBuffer.clear();
        mExtraTextBuffer.reserve(512);

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

    void DebugOverlaySystem::applyTextProperties(
        const core::config::properties::TextProperties& props) {
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
        (void)world;
        (void)dt;
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
        if (mTime) {
            const int fpsRounded = static_cast<int>(mTime->getSmoothedFps());
            mTextBuffer.append("FPS: ");

            std::array<char, 32> buf{};
            auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), fpsRounded);
            if (ec == std::errc{}) {
                mTextBuffer.append(buf.data(), static_cast<std::size_t>(ptr - buf.data()));
            } else {
                mTextBuffer.append("?");
            }
        } else {
            mTextBuffer.append("FPS: ?");
        }

        // ----------------------------------------------------------------------------------------
        // Entity count — показываем ВСЕГДА (O(1) чтение, полезно для диагностики/аналитики)
        // ----------------------------------------------------------------------------------------
        mTextBuffer.append("\nEntities: ");
        core::utils::format::appendU64(mTextBuffer,
                                       static_cast<std::uint64_t>(world.aliveEntityCount()));

        // ----------------------------------------------------------------------------------------
        // Статистика рендера — только Debug/Profile.
        // ----------------------------------------------------------------------------------------
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        if (mRenderSystem) {
            const auto& stats = mRenderSystem->getLastFrameStatsRef();

            mTextBuffer.append("  Sprites: ");
            core::utils::format::appendU64(mTextBuffer,
                                           static_cast<std::uint64_t>(stats.spriteCount));

            mTextBuffer.append("  Vertices: ");
            core::utils::format::appendU64(mTextBuffer,
                                           static_cast<std::uint64_t>(stats.vertexCount));

            mTextBuffer.append("\nDrawCalls: ");
            core::utils::format::appendU64(mTextBuffer,
                                           static_cast<std::uint64_t>(stats.batchDrawCalls));

            mTextBuffer.append("  TexSwitches: ");
            core::utils::format::appendU64(mTextBuffer,
                                           static_cast<std::uint64_t>(stats.textureSwitches));

            mTextBuffer.append("  UniqueTex*: ");
            core::utils::format::appendU64(mTextBuffer,
                                           static_cast<std::uint64_t>(stats.uniqueTexturePointers));

            mTextBuffer.append("  TexCache: ");
            core::utils::format::appendU64(mTextBuffer,
                                           static_cast<std::uint64_t>(stats.textureCacheSize));

            mTextBuffer.append("  SlowResolves: ");
            core::utils::format::appendU64(
                mTextBuffer, static_cast<std::uint64_t>(stats.resourceLookupsThisFrame));

            mTextBuffer.append("\nCulling: ");
            core::utils::format::appendU64(mTextBuffer,
                                           static_cast<std::uint64_t>(stats.spriteCount));
            mTextBuffer.push_back('/');
            core::utils::format::appendU64(mTextBuffer,
                                           static_cast<std::uint64_t>(stats.totalSpriteCount));
            mTextBuffer.push_back('/');
            core::utils::format::appendU64(mTextBuffer,
                                           static_cast<std::uint64_t>(stats.culledSpriteCount));

    #if defined(SFML1_PROFILE)
            // Тайминги CPU — только Profile (Debug не имеет таймингов).
            mSmoothedCpuTotalUs = emaPow2(mSmoothedCpuTotalUs, stats.cpuTotalUs, mSmoothingShift);
            mSmoothedCpuDrawUs  = emaPow2(mSmoothedCpuDrawUs,  stats.cpuDrawUs,  mSmoothingShift);

            mTextBuffer.append("\nCPU: ");
            core::utils::format::appendMs1DecimalFromUs(mTextBuffer, mSmoothedCpuTotalUs);
            mTextBuffer.append(" ms  (draw ");
            core::utils::format::appendMs1DecimalFromUs(mTextBuffer, mSmoothedCpuDrawUs);
            mTextBuffer.append(" ms)");

            // Разбивка статистики RenderSystem: gather/sort/build/draw (в миллисекундах).
            mSmoothedRSGatherUs = emaPow2(mSmoothedRSGatherUs, stats.cpuGatherUs, mSmoothingShift);
            mSmoothedRSSortUs = emaPow2(mSmoothedRSSortUs, stats.cpuSortUs, mSmoothingShift);
            mSmoothedRSBuildUs = emaPow2(mSmoothedRSBuildUs, stats.cpuBuildUs, mSmoothingShift);
            mSmoothedRSDrawUs = emaPow2(mSmoothedRSDrawUs, stats.cpuDrawUs, mSmoothingShift);

            mTextBuffer.append("\nRender(ms): gather ");
            core::utils::format::appendMs1DecimalFromUs(mTextBuffer, mSmoothedRSGatherUs);
            mTextBuffer.append(" | sort ");
            core::utils::format::appendMs1DecimalFromUs(mTextBuffer, mSmoothedRSSortUs);
            mTextBuffer.append(" | build ");
            core::utils::format::appendMs1DecimalFromUs(mTextBuffer, mSmoothedRSBuildUs);
            mTextBuffer.append(" | draw ");
            core::utils::format::appendMs1DecimalFromUs(mTextBuffer, mSmoothedRSDrawUs);
    #endif
        }
#endif // !defined(NDEBUG) || defined(SFML1_PROFILE)

        if (!mExtraTextBuffer.empty()) {
            mTextBuffer.append("\n");
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

#if !defined(NDEBUG)
        const std::size_t needed = 
            mExtraTextBuffer.size() + 
            (mExtraTextBuffer.empty() ? 0u : 1u) + 
            line.size();
        assert(needed <= mExtraTextBuffer.capacity() && 
               "DebugOverlaySystem: mExtraTextBuffer would reallocate. "
               "Increase reserve in bind().");
#endif

        if (!mExtraTextBuffer.empty()) {
            mExtraTextBuffer.push_back('\n');
        }
        mExtraTextBuffer.append(line);
    }

    void DebugOverlaySystem::draw(sf::RenderWindow& window) const {
        if (!mEnabled || !mFpsText) {
            return;
        }
        window.draw(*mFpsText);
    }

} // namespace core::ecs