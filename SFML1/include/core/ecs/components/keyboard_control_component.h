// ================================================================================================
// File: core/ecs/components/keyboard_control_component.h
// Purpose: Per-entity keyboard layout mapping for directional input
// Used by: InputSystem, game-specific init systems
// Related headers: input_system.h, movement_stats_component.h
// ================================================================================================
#pragma once

#include <SFML/Window/Keyboard.hpp>

namespace core::ecs {

    /**
     * @brief Раскладка управления с клавиатуры для конкретной сущности.
     *
     * Важно:
     *  - это чистые данные (какие клавиши считать "вверх/вниз/влево/вправо");
     *  - логика интерпретации нажатий живёт в InputSystem;
     *  - значения по умолчанию (WASD) подходят для прототипа,
     *    но в игре раскладка должна читаться из JSON (data-driven).
     *
     * Таким образом:
     *  - ECS остаётся универсальной;
     *  - конкретная игра (SkyGuard, платформер и т.д.) задаёт раскладку через конфиг.
     */
    struct KeyboardControlComponent {
        sf::Keyboard::Key up = sf::Keyboard::Key::W;
        sf::Keyboard::Key down = sf::Keyboard::Key::S;
        sf::Keyboard::Key left = sf::Keyboard::Key::A;
        sf::Keyboard::Key right = sf::Keyboard::Key::D;
    };

} // namespace core::ecs