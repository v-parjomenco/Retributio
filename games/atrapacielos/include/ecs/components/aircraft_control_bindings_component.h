// ================================================================================================
// File: ecs/components/aircraft_control_bindings_component.h
// Purpose: Game-layer aircraft control bindings (thrust/turn keys)
// Used by: AircraftControlSystem, PlayerInitSystem
// ================================================================================================
#pragma once

#include <SFML/Window/Keyboard.hpp>

namespace game::atrapacielos::ecs {

    /**
     * @brief Привязки клавиш управления кораблём (game-layer).
     */
    struct AircraftControlBindingsComponent {
        sf::Keyboard::Key thrustForward{sf::Keyboard::Key::Unknown};
        sf::Keyboard::Key thrustBackward{sf::Keyboard::Key::Unknown};
        sf::Keyboard::Key turnLeft{sf::Keyboard::Key::Unknown};
        sf::Keyboard::Key turnRight{sf::Keyboard::Key::Unknown};
    };

} // namespace game::atrapacielos::ecs