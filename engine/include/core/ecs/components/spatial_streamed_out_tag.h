// ================================================================================================
// File: core/ecs/components/spatial_streamed_out_tag.h
// Purpose: Marker tag: entity is NOT resident in the spatial streaming window and must be ignored
//          by SpatialIndexSystem write-path. (tag-only, 0 bytes)
// Used by: SpatialStreamingSystem, SpatialIndexSystem.
// ================================================================================================
#pragma once

namespace core::ecs {

    struct SpatialStreamedOutTag final {
    };

} // namespace core::ecs
