#include "pch.h"

#include "core/ecs/systems/debug_overlay_system.h"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <system_error>

#include "core/config/properties/debug_overlay_runtime_properties.h"
#include "core/config/properties/text_properties.h"
#include "core/ecs/systems/render_system.h"
#include "core/time/time_service.h"

namespace {

    void appendU64(std::string& out, std::uint64_t value) {
        std::array<char, 32> buf{};
        auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);
        if (ec == std::errc{}) {
            out.append(buf.data(), static_cast<std::size_t>(ptr - buf.data()));
        } else {
            out.append("?");
        }
    }

    constexpr std::uint64_t kMaxIntAsU64 =
        static_cast<std::uint64_t>(std::numeric_limits<int>::max());
    constexpr std::uint8_t kMaxEmaShift =
static_cast<std::uint8_t>(std::numeric_limits<std::uint64_t>::digits - 1); // 63

#if defined(SFML1_PROFILE)
    void appendMs1DecimalFromUs(std::string& out, std::uint64_t us) {
        // Печатаем миллисекунды с 1 знаком после точки без float:
        // ms10 = (us / 100) == 0.1ms units (с округлением).
        const std::uint64_t ms10 = (us + 50) / 100;

        const std::uint64_t intPart = ms10 / 10;
        const std::uint64_t frac = ms10 % 10;

        appendU64(out, intPart);
        out.push_back('.');
        out.push_back(static_cast<char>('0' + static_cast<int>(frac)));
    }

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
#endif

} // namespace

namespace core::ecs {

    void DebugOverlaySystem::bind(core::time::TimeService& timeService, const sf::Font& font) {
        mTime = &timeService;
        mFpsText.emplace(font);

        mTextBuffer.clear();
        mTextBuffer.reserve(512);

        mRenderClock.restart();
        mAccumulatedTime = mUpdateInterval; // чтобы первый render сразу обновил текст
    }

    void DebugOverlaySystem::applyRuntimeProperties(
        const core::config::properties::DebugOverlayRuntimeProperties& props) noexcept {
        // 0 ms => обновлять каждый кадр.
        if (props.updateIntervalMs == 0) {
            mUpdateInterval = sf::Time::Zero;
        } else {
            const std::uint64_t raw = static_cast<std::uint64_t>(props.updateIntervalMs);
            const int clampedMs = static_cast<int>((raw > kMaxIntAsU64) ? kMaxIntAsU64 : raw);
            mUpdateInterval = sf::milliseconds(clampedMs);
        }

        // ВАЖНО: shift используется в (1ull << shift). shift>=64 => UB.
        mSmoothingShift =
            (props.smoothingShift > kMaxEmaShift) ? kMaxEmaShift : props.smoothingShift;
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
        mFpsText->setCharacterSize(props.characterSize);
        mFpsText->setFillColor(props.color);
    }

    void DebugOverlaySystem::update(World& world, float dt) {
        // Render-only система.
        // Важно: update() может вызываться 0..N раз за кадр (fixed timestep),
        // поэтому любые "per-frame" метрики/строки обновляем в render() ровно один раз за кадр.
        (void)world;
        (void)dt;
    }

    void DebugOverlaySystem::render(World&, sf::RenderWindow& window) {
        if (!mEnabled || !mFpsText) {
            return;
        }

        // Ровно 1 раз за кадр, но строку обновляем реже (убираем дрожь).
        const sf::Time dt = mRenderClock.restart();
        mAccumulatedTime += dt;
        if (mUpdateInterval != sf::Time::Zero && mAccumulatedTime < mUpdateInterval) {
            window.draw(*mFpsText);
            return;
        }
        mAccumulatedTime = sf::Time::Zero;

        mTextBuffer.clear();

        // FPS
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

        // Render stats (если привязали RenderSystem)
        if (mRenderSystem) {
            const auto stats = mRenderSystem->getLastFrameStats();

            mTextBuffer.append("\nSprites: ");
            appendU64(mTextBuffer, static_cast<std::uint64_t>(stats.spriteCount));

            mTextBuffer.append("  Vertices: ");
            appendU64(mTextBuffer, static_cast<std::uint64_t>(stats.vertexCount));

            mTextBuffer.append("\nDrawCalls: ");
            appendU64(mTextBuffer, static_cast<std::uint64_t>(stats.batchDrawCalls));

            mTextBuffer.append("  TexSwitches: ");
            appendU64(mTextBuffer, static_cast<std::uint64_t>(stats.textureSwitches));

            mTextBuffer.append("  UniqueTex*: ");
            appendU64(mTextBuffer, static_cast<std::uint64_t>(stats.uniqueTexturePointers));

            mTextBuffer.append("  TexCache: ");
            appendU64(mTextBuffer, static_cast<std::uint64_t>(stats.textureCacheSize));

            mTextBuffer.append("  SlowResolves: ");
            appendU64(mTextBuffer, static_cast<std::uint64_t>(stats.resourceLookupsThisFrame));

            mTextBuffer.append("\nCulling: ");
            appendU64(mTextBuffer, static_cast<std::uint64_t>(stats.spriteCount));
            mTextBuffer.push_back('/');
            appendU64(mTextBuffer, static_cast<std::uint64_t>(stats.totalSpriteCount));
            mTextBuffer.push_back('/');
            appendU64(mTextBuffer, static_cast<std::uint64_t>(stats.culledSpriteCount));

#if defined(SFML1_PROFILE)
            // Сглаживаем времена, чтобы не прыгали от тика к тику.
            mSmoothedCpuTotalUs = emaPow2(mSmoothedCpuTotalUs, stats.cpuTotalUs, mSmoothingShift);
            mSmoothedCpuDrawUs  = emaPow2(mSmoothedCpuDrawUs,  stats.cpuDrawUs,  mSmoothingShift);

            mTextBuffer.append("\nCPU: ");
            appendMs1DecimalFromUs(mTextBuffer, mSmoothedCpuTotalUs);
            mTextBuffer.append(" ms  (draw ");
            appendMs1DecimalFromUs(mTextBuffer, mSmoothedCpuDrawUs);
            mTextBuffer.append(" ms)");

            // RenderSystem breakdown: raw/sm (в миллисекундах, 1 знак, без float).
            mSmoothedRSGatherUs = emaPow2(mSmoothedRSGatherUs, stats.cpuGatherUs, mSmoothingShift);
            mSmoothedRSSortUs = emaPow2(mSmoothedRSSortUs, stats.cpuSortUs, mSmoothingShift);
            mSmoothedRSBuildUs = emaPow2(mSmoothedRSBuildUs, stats.cpuBuildUs, mSmoothingShift);
            mSmoothedRSDrawUs = emaPow2(mSmoothedRSDrawUs, stats.cpuDrawUs, mSmoothingShift);

            mTextBuffer.append("\nRender(ms): gather ");
            appendMs1DecimalFromUs(mTextBuffer, mSmoothedRSGatherUs);
            mTextBuffer.append(" | sort ");
            appendMs1DecimalFromUs(mTextBuffer, mSmoothedRSSortUs);
            mTextBuffer.append(" | build ");
            appendMs1DecimalFromUs(mTextBuffer, mSmoothedRSBuildUs);
            mTextBuffer.append(" | draw ");
            appendMs1DecimalFromUs(mTextBuffer, mSmoothedRSDrawUs);
#endif
        }

        mFpsText->setString(mTextBuffer);
        window.draw(*mFpsText);
    }

} // namespace core::ecs