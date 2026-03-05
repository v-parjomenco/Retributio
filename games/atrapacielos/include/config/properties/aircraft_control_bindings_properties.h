// ================================================================================================
// File: config/properties/aircraft_control_bindings_properties.h
// Purpose: Data-only aircraft control bindings (thrust/turn keys)
// Used by: PlayerBlueprint, ConfigLoader
// Related headers: (none, pure data brick)
// ================================================================================================
#pragma once

#include <SFML/Window/Keyboard.hpp>

namespace game::atrapacielos::config::properties {

    /**
     * @brief Привязки клавиш управления кораблём (game-layer).
     *
     * Контракт:
     *  - thrustForward/thrustBackward — ускорение вдоль направления;
     *  - turnLeft/turnRight — поворот (в градусах/сек).
     */
    struct AircraftControlBindingsProperties {
        sf::Keyboard::Key thrustForward = sf::Keyboard::Key::W;
        sf::Keyboard::Key thrustBackward = sf::Keyboard::Key::S;
        sf::Keyboard::Key turnLeft = sf::Keyboard::Key::A;
        sf::Keyboard::Key turnRight = sf::Keyboard::Key::D;
    };

} // namespace game::atrapacielos::config::properties