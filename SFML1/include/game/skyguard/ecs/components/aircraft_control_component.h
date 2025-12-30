// ================================================================================================
// File: game/skyguard/ecs/components/aircraft_control_component.h
// Purpose: Game-specific aircraft control parameters
// Used by: AircraftControlSystem, PlayerInitSystem
// ================================================================================================
#pragma once

namespace game::skyguard::ecs {

    /**
     * @brief Параметры управления кораблём (game-layer).
     */
    struct AircraftControlComponent {
        float turnRateDegreesPerSec{0.f};
    };

} // namespace game::skyguard::ecs