// ================================================================================================
// File: core/ecs/components/velocity_component.h
// Purpose: Stores current linear & angular velocity of entity
// Used by: MovementSystem
// Related headers: transform_component.h
// ================================================================================================
#pragma once

#include <SFML/System/Vector2.hpp>

namespace core::ecs {

    /**
     * @brief Компонент скорости.
     *
     * Хранит МГНОВЕННУЮ скорость сущности в мировых координатах:
     *  - linear  — линейная скорость (пикс/с), модуль + направление;
     *  - angular — угловая скорость (°/с), пока не используется,
     *    будет применять вращение к Transform.rotation.
     *
     * Важно:
     *  - это "то, что происходит сейчас", в отличие от MovementStatsComponent,
     *    который задаёт лимиты/возможности;
     *  - MovementSystem интегрирует Velocity в Transform: position += linear * dt.
     */
    struct VelocityComponent {
        sf::Vector2f linear{0.f, 0.f}; // мгновенная линейная скорость, пикс/с
        float angular = 0.f;      // скорость вращения (°/с) - пока не используется
    };

} // namespace core::ecs