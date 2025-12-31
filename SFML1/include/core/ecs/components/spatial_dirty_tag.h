// ================================================================================================
// File: core/ecs/components/spatial_dirty_tag.h
// Purpose: Tag for entities requiring spatial index update
// Used by: MovementSystem, SpatialIndexSystem
// ================================================================================================
#pragma once

namespace core::ecs {

    /**
     * @brief Пустой tag для обновления spatial индекса.
     */
    struct SpatialDirtyTag {};

} // namespace core::ecs