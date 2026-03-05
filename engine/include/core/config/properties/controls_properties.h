// ================================================================================================
// File: core/config/properties/controls_properties.h
// Purpose: Data-only keyboard controls configuration brick
// Used by: PlayerBlueprint, future controllable entity blueprints
// ================================================================================================
#pragma once

#include <SFML/Window/Keyboard.hpp>

namespace core::config::properties {

    /**
     * @brief Набор клавиш управления сущностью.
     *
     * Здесь нет логики input-системы — только «какая клавиша за что отвечает».
     * Логику обработки событий реализует InputSystem + KeyboardControlComponent.
     *
     * JSON — источник истины, эти значения — лишь удобный fallback (WASD).
     */
    struct ControlsProperties {
        sf::Keyboard::Key up = sf::Keyboard::Key::W;
        sf::Keyboard::Key down = sf::Keyboard::Key::S;
        sf::Keyboard::Key left = sf::Keyboard::Key::A;
        sf::Keyboard::Key right = sf::Keyboard::Key::D;
    };

} // namespace core::config::properties