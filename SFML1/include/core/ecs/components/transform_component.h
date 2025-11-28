// ================================================================================================
// File: core/ecs/components/transform_component.h
// Purpose: Stores entity world-space position
// Used by: MovementSystem, RenderSystem
// Related headers: velocity_component.h
// ================================================================================================
#pragma once

#include <SFML/System/Vector2.hpp>

namespace core::ecs {

    /**
     * @brief Простейший трансформ: только позиция в мировых координатах.
     *
     * Сейчас:
     *  - хранится только позиция (sf::Vector2f);
     *  - система координат трактуется как "мир" (world space),
     *    а не экран (screen space).
     *
     * Позже можно расширить до:
     *  - rotation (угол поворота),
     *  - scale (масштаб),
     *  - z-order (слой отрисовки),
     * сохраняя компоненту лёгкой и пригодной для массовых обновлений.
     */
    struct TransformComponent {
        sf::Vector2f position{0.f, 0.f};
    };

} // namespace core::ecs