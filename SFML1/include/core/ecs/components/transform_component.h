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
     * Сейчас:
     *  - хранит только позицию в мировых координатах;
     *  - не содержит визуальных параметров (zOrder/rect/origin/visual scale) — 
     *    это зона render-компонент.
     *
     * Позже можно расширить до:
     *  - rotation (угол поворота), если появится реальная потребность в повороте сущностей;
     *  - scale (масштаб) — только если он станет частью "мирового" трансформа (world-scale),
     *    а не чисто визуальным параметром конкретного рендера.
     *
     * Важный инвариант на будущее:
     *  - если добавляем rotation/scale, то все системы, которые используют bounds
     *    (например culling), обязаны считать их в той же системе координат и по той же математике,
     *    что и отрисовка.
     *
     * Компонента должна оставаться лёгкой и пригодной для массовых обновлений (100K+ сущностей).
     */
    struct TransformComponent {
        sf::Vector2f position{0.f, 0.f};
    };

} // namespace core::ecs