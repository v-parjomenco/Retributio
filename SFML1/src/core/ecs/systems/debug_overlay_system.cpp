#include "core/ecs/systems/debug_overlay_system.h"
#include <string>

#include "core/time/time_service.h"

namespace core::ecs {

    // Связать с сервисом времени и шрифтом (полученным через ResourceManager)
    void DebugOverlaySystem::bind(core::time::TimeService& timeService, const sf::Font& font) {
        mTime = &timeService;
        mFpsText.emplace(font); // инициализация объекта std::optional
    }

    void DebugOverlaySystem::applyStyle(const Style& s) {
        if (!mFpsText) {
            return;
        }
        mFpsText->setPosition(s.position);
        mFpsText->setCharacterSize(s.characterSize);
        mFpsText->setFillColor(s.color);
    }

    void DebugOverlaySystem::update(World&, float) {
        if (!mEnabled || !mTime || !mFpsText) {
            return;
        }

        // Показываем сглаженный FPS (визуально комфортнее)
        const int fpsRounded = static_cast<int>(mTime->getSmoothedFps());
        mFpsText->setString("FPS: " + std::to_string(fpsRounded));
    }

    void DebugOverlaySystem::render(World&, sf::RenderWindow& window) {
        if (!mEnabled || !mFpsText) {
            return;
        }
        window.draw(*mFpsText);
    }

} // namespace core::ecs
