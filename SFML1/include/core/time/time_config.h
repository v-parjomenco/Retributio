// ================================================================================================
// File: core/time/time_config.h
// Purpose: Engine-wide timing constants (fixed timestep, etc.).
// Notes:
//  - These are low-level engine constants, not game-specific settings.
//  - Gameplay tuning (speed, acceleration, etc.) lives in JSON + properties.
// ================================================================================================
#pragma once

#include <SFML/System/Time.hpp>

namespace core::config {

    /**
     * @brief Fixed timestep used by the engine update loop.
     *
     * Логика:
     *  - Игровой цикл вызывает update() с фиксированным шагом логики;
     *  - TimeService управляет накоплением/долгом и количеством апдейтов за кадр;
     *  - графика может обновляться реже/чаще, но логика остаётся детерминированной.
     */
    inline const sf::Time FIXED_TIME_STEP = sf::seconds(1.f / 60.f);

} // namespace core::config