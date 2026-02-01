// ================================================================================================
// File: core/ecs/components/spatial_id_component.h
// Purpose: SpatialId32 mapping + last AABB for SpatialIndexV2 (dirty-on-write)
// Used by: SpatialIndexSystem, RenderSystem
// ================================================================================================
#pragma once

#include <cstdint>

#include "core/spatial/aabb2.h"
#include "core/spatial/entity_id32.h"

namespace core::ecs {

    /**
     * @brief SpatialId32 + last AABB for SpatialIndexV2 updates.
     *
     * SpatialId32:
     *  - O(1) dense mapping to Entity via SpatialIndexSystem.
     *  - 0 reserved as invalid/unassigned.
     */
    struct SpatialIdComponent {
        core::spatial::EntityId32 id = 0;
        core::spatial::Aabb2 lastAabb{};
    };

} // namespace core::ecs