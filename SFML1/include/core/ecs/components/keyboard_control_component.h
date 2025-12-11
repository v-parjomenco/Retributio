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
     *  - реальные значения должны приходить из конфигов/blueprint'ов
     *    (core::config::properties::ControlsProperties), а не из дефолтов компонента.
     *
     * По умолчанию компонента — "пустая" (Unknown). Это сознательное решение:
     *  - единственный источник дефолтов (WASD) — конфиговый слой;
     *  - init-система обязана заполнить поля при создании сущности.
     *
     * Таким образом:
     *  - ECS остаётся универсальной;
     *  - конкретная игра задаёт раскладку через JSON (data-driven).
     */
    struct KeyboardControlComponent {
        sf::Keyboard::Key up = sf::Keyboard::Key::Unknown;
        sf::Keyboard::Key down = sf::Keyboard::Key::Unknown;
        sf::Keyboard::Key left = sf::Keyboard::Key::Unknown;
        sf::Keyboard::Key right = sf::Keyboard::Key::Unknown;
    };

} // namespace core::ecs