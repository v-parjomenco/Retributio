// ================================================================================================
// File: config/properties/aircraft_control_properties.h
// Purpose: Data-only aircraft control configuration (turn rate, initial rotation)
// Used by: PlayerBlueprint, AircraftControlSystem
// Related headers: (none, pure data brick)
// ================================================================================================
#pragma once

namespace game::atrapacielos::config::properties {

    /**
     * @brief Параметры управления кораблём (game-layer).
     *
     * Контракт:
     *  - turnRateDegreesPerSec — скорость поворота (°/с), clockwise = положительно;
     *  - initialRotationDegrees — начальный угол (°), 0° = East (+X).
     */
    struct AircraftControlProperties {
        float turnRateDegreesPerSec = 180.f;
        float initialRotationDegrees = 0.f;
    };

} // namespace game::atrapacielos::config::properties