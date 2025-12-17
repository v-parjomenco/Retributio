#include "pch.h"

#include "core/ecs/systems/debug_overlay_system.h"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <system_error>

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

    void appendMs1DecimalFromUs(std::string& out, std::uint64_t us) {
        // Печатаем миллисекунды с 1 знаком после точки без float:
        // ms10 = (us / 100) == 0.1ms units (с округлением).
        const std::uint64_t ms10 = (us + 50) / 100;

        const std::uint64_t intPart = ms10 / 10;
        const std::uint64_t frac    = ms10 % 10;

        appendU64(out, intPart);
        out.push_back('.');
        out.push_back(static_cast<char>('0' + static_cast<int>(frac)));
    }

} // namespace

namespace core::ecs {

    void DebugOverlaySystem::bind(core::time::TimeService& timeService, const sf::Font& font) {
        mTime = &timeService;
        mFpsText.emplace(font);

        mTextBuffer.clear();
        mTextBuffer.reserve(256);
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

            mTextBuffer.append("  Verts: ");
            appendU64(mTextBuffer, static_cast<std::uint64_t>(stats.vertexCount));

            mTextBuffer.append("\nBatches: ");
            appendU64(mTextBuffer, static_cast<std::uint64_t>(stats.batchDrawCalls));

            mTextBuffer.append("  TexSw: ");
            appendU64(mTextBuffer, static_cast<std::uint64_t>(stats.textureSwitches));

            mTextBuffer.append("\nRender: ");
            appendMs1DecimalFromUs(mTextBuffer, stats.cpuTotalUs);
            mTextBuffer.append(" ms  (draw ");
            appendMs1DecimalFromUs(mTextBuffer, stats.cpuDrawUs);
            mTextBuffer.append(" ms)");
        }

        mFpsText->setString(mTextBuffer);
        window.draw(*mFpsText);
    }

} // namespace core::ecs