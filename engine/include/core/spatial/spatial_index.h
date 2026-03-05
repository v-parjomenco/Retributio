// ================================================================================================
// File: core/spatial/spatial_index.h
// Purpose: Grid-based spatial index with dirty-on-write updates
// Used by: SpatialIndexSystem, RenderSystem
// ================================================================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

#include "core/ecs/entity.h"
#include "core/spatial/aabb2.h"

namespace core::spatial {

    class SpatialIndex final {
      public:
        explicit SpatialIndex(float cellSize = 256.f) noexcept;

        [[nodiscard]] float cellSize() const noexcept {
            return mCellSize;
        }

        [[nodiscard]] float invCellSize() const noexcept {
            return mInvCellSize;
        }

        [[nodiscard]] std::uint32_t registerEntity(core::ecs::Entity entity,
                                                   const Aabb2& bounds);

        void update(std::uint32_t handle, const Aabb2& oldBounds, const Aabb2& newBounds);

        void unregister(std::uint32_t handle, const Aabb2& lastBounds) noexcept;

        void query(const Aabb2& area, std::vector<core::ecs::Entity>& out) const;

      private:
        struct CellCoord {
            int x = 0;
            int y = 0;
        };

        struct CellCoordHash {
            std::size_t operator()(const CellCoord& c) const noexcept {
                const std::size_t hx = std::hash<int>{}(c.x);
                const std::size_t hy = std::hash<int>{}(c.y);
                return hx ^ (hy + 0x9e3779b9 + (hx << 6) + (hx >> 2));
            }
        };

        struct CellCoordEq {
            bool operator()(const CellCoord& a, const CellCoord& b) const noexcept {
                return a.x == b.x && a.y == b.y;
            }
        };

        using Cell = std::vector<std::uint32_t>; // хранит только handle

        struct CellRange {
            int minX = 0;
            int minY = 0;
            int maxX = 0;
            int maxY = 0;
        };

        [[nodiscard]] CellRange computeCellRange(const Aabb2& bounds) const noexcept;

        void insertIntoCells(std::uint32_t handle, const CellRange& range);

        void eraseFromCells(std::uint32_t handle, const CellRange& range) noexcept;

        float mCellSize = 256.f;
        float mInvCellSize = 1.f / 256.f;

        std::uint32_t mNextHandle = 1;

        std::unordered_map<CellCoord, Cell, CellCoordHash, CellCoordEq> mCells;

        std::vector<core::ecs::Entity> mHandleToEntity;
        mutable std::vector<std::uint32_t> mQueryMarks;
        mutable std::uint32_t mQueryStamp = 1;
    };

} // namespace core::spatial