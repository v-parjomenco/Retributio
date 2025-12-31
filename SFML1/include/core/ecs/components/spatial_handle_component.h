// ================================================================================================
// File: core/ecs/components/spatial_handle_component.h
// Purpose: Spatial index handle + last AABB (dirty-on-write)
// Used by: SpatialIndexSystem
// ================================================================================================
#pragma once

#include <cstdint>

#include "core/spatial/aabb2.h"

namespace core::ecs {

    /**
     * @brief Хэндл в SpatialIndex и последняя AABB для update/unregister.
     */
    struct SpatialHandleComponent {
        std::uint32_t handle = 0;
        core::spatial::Aabb2 lastAabb{};
    };

} // namespace core::ecs