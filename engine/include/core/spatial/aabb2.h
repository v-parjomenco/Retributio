// ================================================================================================
// File: core/spatial/aabb2.h
// Purpose: Lightweight axis-aligned bounding box (2D)
// Used by: SpatialIndex, Render/visibility systems
// Related headers: core/spatial/aabb2i.h
//
// Notes:
//  - Inclusive semantics; min==max is a valid point, not empty.
// ================================================================================================
#pragma once

namespace core::spatial {

    /**
     * @brief Простая 2D AABB (min/max) в world-space.
     * Семантика: inclusive границы; min==max — валидная точка, не пусто.
     */
    struct Aabb2 {
        float minX = 0.f;
        float minY = 0.f;
        float maxX = 0.f;
        float maxY = 0.f;
    };

    /**
     * @brief Инклюзивное пересечение (касание границ считается пересечением).
     * Семантика: инклюзивные границы; min==max => point.
     */
    [[nodiscard]] inline bool intersectsInclusive(const Aabb2& a, const Aabb2& b) noexcept {
        return (a.maxX >= b.minX) && (a.minX <= b.maxX) &&
               (a.maxY >= b.minY) && (a.minY <= b.maxY);
    }

} // namespace core::spatial