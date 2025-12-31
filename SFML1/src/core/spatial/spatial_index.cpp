#include "pch.h"

#include "core/spatial/spatial_index.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace {

    [[nodiscard]] int toCellCoord(const float value, const float invCellSize) noexcept {
        return static_cast<int>(std::floor(value * invCellSize));
    }

} // namespace

namespace core::spatial {

    SpatialIndex::SpatialIndex(float cellSize) noexcept
        : mCellSize(cellSize)
        , mInvCellSize(1.f / cellSize)
        , mNextHandle(1) {
#if !defined(NDEBUG)
        assert(cellSize >= 64.f && cellSize <= 2048.f &&
               "SpatialIndex: cellSize out of allowed range [64..2048]");
#endif
    }

    std::uint32_t SpatialIndex::registerEntity(core::ecs::Entity entity, const Aabb2& bounds) {
        const std::uint32_t handle = mNextHandle++;

#if !defined(NDEBUG)
        assert(handle != 0 && "SpatialIndex: handle overflow");
#endif

        if (handle >= mQueryMarks.size()) {
            const std::size_t newSize = static_cast<std::size_t>(handle) + 1u;
            mQueryMarks.resize(newSize, 0u);
            mHandleToEntity.resize(newSize, core::ecs::NullEntity);
        }

        mHandleToEntity[handle] = entity;

        const CellRange range = computeCellRange(bounds);
        insertIntoCells(handle, range);
        return handle;
    }

    void SpatialIndex::update(std::uint32_t handle, const Aabb2& oldBounds,
                              const Aabb2& newBounds) {
#if !defined(NDEBUG)
        assert(handle < mHandleToEntity.size() &&
               "SpatialIndex::update: handle out of range");
#endif
        const CellRange oldRange = computeCellRange(oldBounds);
        const CellRange newRange = computeCellRange(newBounds);

        if (oldRange.minX == newRange.minX && oldRange.minY == newRange.minY &&
            oldRange.maxX == newRange.maxX && oldRange.maxY == newRange.maxY) {
            return;
        }

        eraseFromCells(handle, oldRange);
        insertIntoCells(handle, newRange);
    }

    void SpatialIndex::unregister(std::uint32_t handle, const Aabb2& lastBounds) noexcept {
#if !defined(NDEBUG)
        assert(handle < mHandleToEntity.size() &&
               "SpatialIndex::unregister: handle out of range");
#endif
        const CellRange range = computeCellRange(lastBounds);
        eraseFromCells(handle, range);
        if (handle < mHandleToEntity.size()) {
            mHandleToEntity[handle] = core::ecs::NullEntity;
        }
    }

    void SpatialIndex::query(const Aabb2& area, std::vector<core::ecs::Entity>& out) const {
        const CellRange range = computeCellRange(area);

        if (++mQueryStamp == 0) {
            std::fill(mQueryMarks.begin(), mQueryMarks.end(), 0u);
            mQueryStamp = 1;
        }

        for (int y = range.minY; y <= range.maxY; ++y) {
            for (int x = range.minX; x <= range.maxX; ++x) {
                const CellCoord coord{x, y};
                const auto it = mCells.find(coord);
                if (it == mCells.end()) {
                    continue;
                }

                const auto& entries = it->second;
                for (const std::uint32_t handle : entries) {
#if !defined(NDEBUG)
                    assert(handle < mQueryMarks.size() && 
                        "SpatialIndex::query: corrupted handle in cell");
#endif
                    if (mQueryMarks[handle] == mQueryStamp) {
                        continue;
                    }

                    mQueryMarks[handle] = mQueryStamp;
                    const core::ecs::Entity e = mHandleToEntity[handle];
#if !defined(NDEBUG)
                    assert(e != core::ecs::NullEntity && 
                        "SpatialIndex::query: handle->entity is NullEntity");
#endif
                    out.push_back(e);
                }
            }
        }
    }

    SpatialIndex::CellRange SpatialIndex::computeCellRange(const Aabb2& bounds) const noexcept {
        CellRange range{};
        range.minX = toCellCoord(bounds.minX, mInvCellSize);
        range.minY = toCellCoord(bounds.minY, mInvCellSize);
        range.maxX = toCellCoord(bounds.maxX, mInvCellSize);
        range.maxY = toCellCoord(bounds.maxY, mInvCellSize);
        return range;
    }

    void SpatialIndex::insertIntoCells(std::uint32_t handle, const CellRange& range) {
        for (int y = range.minY; y <= range.maxY; ++y) {
            for (int x = range.minX; x <= range.maxX; ++x) {
                const CellCoord coord{x, y};
                auto& cell = mCells[coord];
#if !defined(NDEBUG)
                const core::ecs::Entity entity = mHandleToEntity[handle];
                assert(entity != core::ecs::NullEntity &&
                       "SpatialIndex: handle->entity mapping is invalid");
#endif
                cell.push_back(handle);
            }
        }
    }

    void SpatialIndex::eraseFromCells(std::uint32_t handle, const CellRange& range) noexcept {
        for (int y = range.minY; y <= range.maxY; ++y) {
            for (int x = range.minX; x <= range.maxX; ++x) {
                const CellCoord coord{x, y};
                auto it = mCells.find(coord);
                if (it == mCells.end()) {
                    continue;
                }

                auto& handles = it->second;
                for (std::size_t i = 0; i < handles.size(); ++i) {
                    if (handles[i] == handle) {
                        handles[i] = handles.back();
                        handles.pop_back();
                        break;
                    }
                }
            }
        }
    }

} // namespace core::spatial