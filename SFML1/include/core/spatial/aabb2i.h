// ================================================================================================
// File: core/spatial/aabb2i.h
// Purpose: Integer axis-aligned bounding box (2D, half-open)
// Used by: SpatialIndex v2 (Titan), deterministic simulation
// Related headers: (none)
// Notes: Half-open bounds [min, max). min==max is empty.
// ================================================================================================
#pragma once

#include <cstdint>

namespace core::spatial {

    /**
     * @brief Целочисленная 2D AABB (min/max) в world-space.
     * Семантика: half-open [min, max).
     */
    struct Aabb2i final {
        std::int32_t minX = 0;
        std::int32_t minY = 0;
        std::int32_t maxX = 0;
        std::int32_t maxY = 0;
    };

    /**
     * @brief Пересечение half-open: пересечение есть, если площади пересекаются ненулево.
     */
    [[nodiscard]] inline bool intersectsHalfOpen(const Aabb2i& a, const Aabb2i& b) noexcept {
        return (a.maxX > b.minX) && (a.minX < b.maxX) && (a.maxY > b.minY) && (a.minY < b.maxY);
    }

} // namespace core::spatial