// ================================================================================================
// File: core/config/properties/controls_properties.h
// Purpose: Data-only keyboard controls configuration brick
// Used by: PlayerBlueprint, future controllable entity blueprints
// Related headers: core/config.h
// ================================================================================================
#pragma once

#include <SFML/Window/Keyboard.hpp>

#include "core/config.h"

namespace core::config::properties {

    /**
 * @brief Набор клавиш управления сущностью.
 *
 * Здесь нет логики input-системы — только «какая клавиша за что отвечает».
 * Логику обработки событий реализует InputSystem + KeyboardControlComponent.
 */
    struct ControlsProperties {
        sf::Keyboard::Key up = ::config::DEFAULT_KEY_UP;
        sf::Keyboard::Key down = ::config::DEFAULT_KEY_DOWN;
        sf::Keyboard::Key left = ::config::DEFAULT_KEY_LEFT;
        sf::Keyboard::Key right = ::config::DEFAULT_KEY_RIGHT;
    };

} // namespace core::config::properties