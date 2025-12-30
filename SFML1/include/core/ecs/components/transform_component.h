// ================================================================================================
// File: core/ecs/components/transform_component.h
// Purpose: Stores entity world-space position (and later: rotation / optional world-scale).
// Used by: MovementSystem, RenderSystem, и иными spatial-системами (culling/ai/spatial)
// Related headers: velocity_component.h
// ================================================================================================
#pragma once

#include <SFML/System/Vector2.hpp>

namespace core::ecs {

    /**
     * @brief Minimal world-space transform for ECS entities.
     *
     * Контракт координат/углов:
     *  - position — мировая позиция pivot-точки (origin) спрайта;
     *  - rotationDegrees — угол в градусах, 0° = East (+X), рост по часовой (Y-down).
     *
     * Визуальные параметры (zOrder/rect/origin/visual scale) живут в SpriteComponent.
     *
     * Компонента должна оставаться лёгкой и пригодной для массовых обновлений (100K+ сущностей).
     */
    struct TransformComponent {
        sf::Vector2f position{0.f, 0.f};
        float rotationDegrees{0.f};
    };

} // namespace core::ecs