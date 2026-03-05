// ================================================================================================
// File: core/ecs/components/movement_stats_component.h
// Purpose: Per-entity movement capabilities and limits
// Used by: InputSystem, MovementSystem, game-specific init systems
// Related headers: velocity_component.h, core/config/properties/movement_properties.h
// ================================================================================================
#pragma once

namespace core::ecs {

    /**
     * @brief Статические параметры движения (возможности сущности).
     *
     * Это НЕ "то, что она делает сейчас", а "то, на что она способна":
     *  - maxSpeed     — ограничение по модулю скорости (пикс/с);
     *  - acceleration — как быстро сущность набирает скорость;
     *  - friction     — как быстро сбрасывает.
     *
     * Источник данных:
     *  - значения должны приходить из core::config::properties::MovementProperties
     *    (JSON/blueprint), обычно в init-системах (например, PlayerInitSystem);
     *  - дефолты компонента равны 0, чтобы не дублировать игровую "магическую"
     *    константу и не маскировать ошибки, если кто-то забыл инициализировать
     *    компонент (лучше сущность не двинется, чем полетит с сюрпризом).
     *
     * Сейчас часть полей может не использоваться (ускорение/трение),
     * но модель уже заложена под более физически правдоподобное движение.
     */
    struct MovementStatsComponent {
        float maxSpeed = 0.f;
        float acceleration = 0.f;
        float friction = 0.f;
    };

} // namespace core::ecs