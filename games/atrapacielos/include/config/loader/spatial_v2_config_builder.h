// ================================================================================================
// File: config/loader/spatial_v2_config_builder.h
// Purpose: Build authoritative SpatialIndexV2 config for Atrapacielos (contract-backed).
// Used by: game::atrapacielos::Game (initWorld)
// ================================================================================================
#pragma once

#include <cstddef>

#include <SFML/System/Vector2.hpp>

#include "core/config/engine_settings.h"
#include "core/ecs/systems/spatial_index_system.h"

namespace game::atrapacielos::config {

    /**
     * @brief Build SpatialIndexV2 config for Atrapacielos (SlidingWindowStorage).
     *
     * Контракт:
     *  - Fail-fast на несогласованных параметрах.
     *  - Один источник правды для SpatialIndexV2 в Atrapacielos.
     */
    [[nodiscard]] core::ecs::SpatialIndexSystemConfig buildSpatialIndexV2ConfigAtrapacielos(
        const core::config::EngineSettings& settings,
        const sf::Vector2f worldLogicalSize,
        const sf::Vector2u windowPixelSize);

    // StableIdService capacity planning for deterministic mode (Atrapacielos wiring).
    // Возвращает CAPACITY таблиц StableIdService (индексация по entt::to_entity(e)).
    // Вызывается в cold-path перед первым тиком, дальше таблицы "заморожены".
    [[nodiscard]] std::size_t computeStableIdCapacityAtrapacielos(
        const core::ecs::SpatialIndexSystemConfig& spatialCfg) noexcept;

} // namespace game::atrapacielos::config