#include "pch.h"

#include "core/ecs/systems/debug_overlay_system.h"

#include "core/config/properties/text_properties.h"
#include "core/time/time_service.h"

namespace core::ecs {

    // Связать с сервисом времени и шрифтом (полученным через ResourceManager)
    void DebugOverlaySystem::bind(core::time::TimeService& timeService, const sf::Font& font) {
        mTime = &timeService;
        mFpsText.emplace(font); // инициализация объекта std::optional
    }

    void
    DebugOverlaySystem::applyTextProperties(const core::config::properties::TextProperties& props) {
        if (!mFpsText) {
            return;
        }
        mFpsText->setPosition(props.position);
        mFpsText->setCharacterSize(props.characterSize);
        mFpsText->setFillColor(props.color);
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
