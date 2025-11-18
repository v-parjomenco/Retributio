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
     * Хранит мгновенную скорость сущности в мировых координатах.
     * То, что сейчас делает объект.
     */
    struct VelocityComponent {
        sf::Vector2f linear{0.f,
                            0.f}; // мгновенная линейная скорость (модуль + направление), пикс/с
        float angular = 0.f;      // скорость вращения (°/с) - пока не используется, будет
                                  // применять вращение к Transform.rotation
    };

} // namespace core::ecs