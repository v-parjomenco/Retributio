// ================================================================================================
// File: core/ecs/components/movement_stats_component.h
// Purpose: Per-entity movement capabilities and limits
// Used by: InputSystem, MovementSystem, game-specific init systems
// Related headers: velocity_component.h, keyboard_control_component.h
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
     * Сейчас часть полей может не использоваться (ускорение/трение),
     * но модель уже заложена под более физически правдоподобное движение.
     *
     * Логика интерпретации:
     *  - InputSystem решает, в каком направлении двигаться;
     *  - MovementSystem и/или отдельная PhysicSystem могут использовать
     *    эти параметры при обновлении VelocityComponent.
     *
     * Все значения должны приходить из JSON-конфигов конкретной игры
     * (data-driven), а эти дефолты — только безопасные значения "по умолчанию"
     * для прототипов и тестов.
     */
    struct MovementStatsComponent {
        float maxSpeed = 700.f;     // максимальная скорость, пикс/с
        float acceleration = 800.f; // ускорение, пикс/с^2 (пока не используется)
        float friction = 6.f;       // коэффициент замедления при отпускании клавиш
                                    // (пока не используется)
    };

} // namespace core::ecs