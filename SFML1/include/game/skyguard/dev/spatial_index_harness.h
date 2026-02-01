// ================================================================================================
// File: game/skyguard/dev/spatial_index_harness.h
// Purpose: Debug/Profile-only harness comparing SpatialIndex V1 vs V2 correctness + timing.
// ================================================================================================
#pragma once

#include "core/ecs/systems/spatial_index_system.h"

namespace game::skyguard::dev {

    void tryRunSpatialIndexHarnessFromEnv(const core::ecs::SpatialIndexSystemConfig& spatialCfg);

} // namespace game::skyguard::dev