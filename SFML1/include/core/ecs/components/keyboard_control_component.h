#pragma once

#include <SFML/Window/Keyboard.hpp>

namespace core::ecs {

    /**
     * @brief Раскладка управления с клавиатуры для сущности.
     * Можно расширить (dash, jump, inventory и т.д.), а также
     * читать её из JSON (data-driven).
     */
    struct KeyboardControlComponent {
        sf::Keyboard::Key up = sf::Keyboard::Key::W;
        sf::Keyboard::Key down = sf::Keyboard::Key::S;
        sf::Keyboard::Key left = sf::Keyboard::Key::A;
        sf::Keyboard::Key right = sf::Keyboard::Key::D;
    };

} // namespace core::ecs