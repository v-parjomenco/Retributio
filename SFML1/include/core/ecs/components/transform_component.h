#pragma once

#include <SFML/System/Vector2.hpp>

namespace core::ecs {

    /**
     * @brief Простейший трансформ: только позиция.
     * Можно расширить до rotation / scale / z-order позже.
     */
    struct TransformComponent {
        sf::Vector2f position{ 0.f, 0.f };
    };

} // namespace core::ecs