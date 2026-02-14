// ================================================================================================
// File: core/spatial/spatial_index_v2.h
// Purpose: SpatialIndex v2 (chunked, cache-line cells, deterministic overflow)
// Used by: Stage 3 core library (no ECS integration yet)
// Notes:
//  - Stage 4 readiness: storages use residency-bounded cell-block pool
//    (allocate on Loaded, free on leave Loaded).
//  - Two query entry points only: queryFast(), queryDeterministic().
// ================================================================================================
#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>
#include <vector>

#include "core/log/log_macros.h"
#include "core/spatial/aabb2.h"
#include "core/spatial/aabb2i.h"
#include "core/spatial/chunk_coord.h"
#include "core/spatial/entity_id32.h"

namespace core::spatial {

    enum class ResidencyState : std::uint8_t { Unloaded = 0, Loading, Loaded, Unloading };

    inline constexpr std::uint32_t kInvalidCellsBlockIndex =
        std::numeric_limits<std::uint32_t>::max();

    namespace detail {

        // Euclidean modulo: always returns [0..b-1] for b > 0.
        // Используется для ring-buffer индексации SlidingWindowStorage.
        [[nodiscard]] inline std::int32_t euclideanMod(const std::int32_t a,
                                                       const std::int32_t b) noexcept {
            const std::int32_t r = a % b;
            return (r >= 0) ? r : (r + b);
        }

    } // namespace detail

    struct alignas(64) Cell final {
        static constexpr std::size_t kCacheLineBytes = 64;
        static constexpr std::size_t kOverheadBytes = 1 + 3 + 4; // count + padding + handle
        static constexpr std::uint8_t kInlineCapacity =
            static_cast<std::uint8_t>((kCacheLineBytes - kOverheadBytes) / sizeof(EntityId32));

        std::array<EntityId32, kInlineCapacity> entities{}; // 56 bytes
        std::uint8_t count = 0;
        std::uint8_t pad0 = 0;
        std::uint8_t pad1 = 0;
        std::uint8_t pad2 = 0;
        std::uint32_t overflowHandle = 0; // 0 == none
    };

    static_assert(Cell::kInlineCapacity == 14,
                  "Cell::kInlineCapacity must equal 14 (cache-line packing contract)");
    static_assert(sizeof(Cell) == 64, "Cell must fit exactly one cache line.");
    static_assert(alignof(Cell) == 64, "Cell must align to cache-line boundary.");

struct SpatialChunk final {
        std::uint32_t cellsBlockIndex = kInvalidCellsBlockIndex; // cell-offset in pool, or invalid

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
         // Число зарегистрированных сущностей, чьи AABB пересекают этот chunk (coverage).
         // НЕ membership/active-set; стриминг владеет residency/ceilings и "кто реально активен".
         std::uint32_t aabbOverlapCount = 0;
#endif

        ResidencyState state = ResidencyState::Unloaded;
    };

    struct OverflowConfig final {
        std::uint32_t nodeCapacity = 32;
        std::uint32_t maxNodes = 0;
    };

    struct SpatialIndexConfig final {
        std::int32_t chunkSizeWorld = 0;
        std::int32_t cellSizeWorld = 0;

        // Legacy/alias (kept for compatibility). Stage 3/4 contract uses maxEntityId strictly.
        std::uint32_t marksCapacity = 0;

        std::uint32_t maxEntityId = 0; // MUST be set (>0). Governs prewarm for marks/entities.
        OverflowConfig overflow{};
    };

    // Контракт write-path: атомарный. Entity либо полностью записана во все cells, либо отвергнута.
    // Частичная запись (truncation) запрещена by design.
    enum class WriteResult : std::uint8_t { Success = 0, Rejected };

    struct FlatStorageConfig final {
        ChunkCoord origin{0, 0};
        std::int32_t width = 0;
        std::int32_t height = 0;

        // Stage 4: max simultaneously resident (Loaded) chunks.
        // 0 => "no cap" (falls back to width*height; Stage 3 behavior).
        std::uint32_t maxResidentChunks = 0;
    };

    struct SlidingWindowConfig final {
        ChunkCoord origin{0, 0};
        std::int32_t width = 0;
        std::int32_t height = 0;

        // Stage 4: max simultaneously resident (Loaded) chunks within the window.
        // 0 => "no cap" (falls back to width*height).
        std::uint32_t maxResidentChunks = 0;
    };

    class OverflowPool final {
      public:
        void init(const OverflowConfig& cfg);

        [[nodiscard]] std::uint32_t nodeCapacity() const noexcept {
            return mNodeCapacity;
        }

        [[nodiscard]] std::uint32_t maxNodes() const noexcept {
            return mMaxNodes;
        }

        [[nodiscard]] bool append(std::uint32_t& headHandle, EntityId32 id);
        [[nodiscard]] bool remove(std::uint32_t& headHandle, EntityId32 id) noexcept;
        void releaseChain(std::uint32_t& headHandle) noexcept;

        template <typename Fn> bool forEach(std::uint32_t headHandle, Fn&& fn) const;

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        struct DebugStats final {
            std::uint32_t nodeCapacity = 0;
            std::uint32_t maxNodes = 0;
            std::uint32_t peakUsedNodes = 0;
            std::uint32_t freeDepth = 0;
        };

        [[nodiscard]] DebugStats debugStats() const noexcept;
        [[nodiscard]] std::uint32_t totalCount(std::uint32_t headHandle) const noexcept;
#endif

      private:
        [[nodiscard]] std::uint32_t allocateNode();
        void releaseNode(const std::uint32_t handle) noexcept;

        [[nodiscard]] std::uint32_t nodeIndex(const std::uint32_t handle) const noexcept {
            return handle - 1u;
        }

        [[nodiscard]] std::uint32_t handleFromIndex(const std::uint32_t idx) const noexcept {
            return idx + 1u;
        }

        // Возвращает size_t — расширяем ДО умножения, исключая переполнение uint32
        // при больших пулах (mMaxNodes * mNodeCapacity > UINT32_MAX).
        [[nodiscard]] std::size_t entryOffset(const std::uint32_t idx) const noexcept {
            return static_cast<std::size_t>(idx) * mNodeCapacity;
        }

        std::uint32_t mNodeCapacity = 0;
        std::uint32_t mMaxNodes = 0;
        std::uint32_t mNextUnusedIndex = 0;
        std::uint32_t mFreeCount = 0;
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        std::uint32_t mPeakUsedNodes = 0;
#endif

        std::vector<EntityId32> mEntries{};
        std::vector<std::uint32_t> mNext{};
        std::vector<std::uint32_t> mCounts{};
        std::vector<std::uint32_t> mTailByHead{};
        std::vector<std::uint32_t> mFreeStack{};
    };

    namespace detail {

        // R11: общая очистка cell-блока (DRY, без touching entities[]).
        inline void clearChunkCells(Cell* cells, const std::size_t cellCount,
                                    OverflowPool* pool) noexcept {
            if (cells == nullptr) {
                return;
            }

            for (std::size_t i = 0; i < cellCount; ++i) {
                Cell& cell = cells[i];
                cell.count = 0;

                if (cell.overflowHandle != 0u) {
                    if (pool != nullptr) {
                        pool->releaseChain(cell.overflowHandle);
                    }
                    cell.overflowHandle = 0u;
                }
            }
        }

        inline void resetChunkOverlapCount(SpatialChunk& chunk) noexcept {
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
            chunk.aabbOverlapCount = 0;
#else
            (void) chunk;
#endif
        }

        inline void incrementChunkOverlapCount(SpatialChunk& chunk) noexcept {
#if !defined(NDEBUG)
            assert(chunk.aabbOverlapCount < std::numeric_limits<std::uint32_t>::max() &&
                   "SpatialIndexV2: aabbOverlapCount overflow (debug-only)");
             ++chunk.aabbOverlapCount;
#elif defined(SFML1_PROFILE)
            if (chunk.aabbOverlapCount == std::numeric_limits<std::uint32_t>::max()) [[unlikely]] {
                LOG_PANIC(core::log::cat::ECS,
                          "SpatialIndexV2: aabbOverlapCount overflow (profile)");
            }
            ++chunk.aabbOverlapCount;
#else
            (void) chunk;
#endif
        }

        inline void decrementChunkOverlapCount(SpatialChunk& chunk) noexcept {
#if !defined(NDEBUG)
            assert(chunk.aabbOverlapCount > 0 &&
                   "SpatialIndexV2: aabbOverlapCount underflow (debug-only)");
            --chunk.aabbOverlapCount;
#elif defined(SFML1_PROFILE)
            if (chunk.aabbOverlapCount == 0u) [[unlikely]] {
                LOG_PANIC(core::log::cat::ECS,
                          "SpatialIndexV2: aabbOverlapCount underflow (profile)");
            }
            --chunk.aabbOverlapCount;
#else
            (void) chunk;
#endif
        }

        [[nodiscard]] inline bool chunkOverlapCountIsZero(const SpatialChunk& chunk) noexcept {
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
            return chunk.aabbOverlapCount == 0;
#else
            (void) chunk;
            return true;
#endif
        }

        [[nodiscard]] inline std::uint32_t chunkOverlapCountForLog(
            const SpatialChunk& chunk) noexcept {
        #if !defined(NDEBUG) || defined(SFML1_PROFILE)
            return chunk.aabbOverlapCount;
        #else
            (void) chunk;
            return 0u;
        #endif
        }

    } // namespace detail

    class FlatStorage final {
      public:
        using Config = FlatStorageConfig;

        void bindOverflowPool(OverflowPool* pool) noexcept {
            mOverflowPool = pool;
        }
        void init(const Config& config, const std::int32_t cellsPerChunkX,
                  const std::int32_t cellsPerChunkY);

        [[nodiscard]] SpatialChunk* tryGetChunk(const ChunkCoord coord) noexcept;
        [[nodiscard]] const SpatialChunk* tryGetChunk(const ChunkCoord coord) const noexcept;

        template <typename Fn>
        bool forEachChunkInRange(const ChunkCoord minCoord, const ChunkCoord maxCoord,
                                 Fn&& fn) const;

        template <typename Fn>
        bool forEachChunkInRangeMut(const ChunkCoord minCoord, const ChunkCoord maxCoord,
                                    Fn&& fn);

        [[nodiscard]] bool setChunkState(const ChunkCoord coord, const ResidencyState state);
        [[nodiscard]] ResidencyState chunkState(const ChunkCoord coord) const noexcept;
        [[nodiscard]] Cell* cellsForChunk(SpatialChunk& chunk) noexcept;
        [[nodiscard]] const Cell* cellsForChunk(const SpatialChunk& chunk) const noexcept;

        [[nodiscard]] ChunkCoord origin() const noexcept {
            return mOrigin;
        }

        [[nodiscard]] std::int32_t width() const noexcept {
            return mWidth;
        }

        [[nodiscard]] std::int32_t height() const noexcept {
            return mHeight;
        }

      private:
        [[nodiscard]] bool isInBounds(const ChunkCoord coord) const noexcept;
        [[nodiscard]] std::size_t indexOf(const ChunkCoord coord) const noexcept;

        [[nodiscard]] std::uint32_t allocateCellsBlock();
        void releaseCellsBlock(const std::uint32_t cellsBlockIndex) noexcept;

        void clearChunkData(SpatialChunk& chunk) noexcept;

        ChunkCoord mOrigin{0, 0};
        std::int32_t mWidth = 0;
        std::int32_t mHeight = 0;

        // Precomputed inclusive bounds (avoid signed overflow in hot-path clamps).
        std::int32_t mMaxXInclusive = 0;
        std::int32_t mMaxYInclusive = 0;

        // Cells per chunk (precomputed once; avoids divisions).
        std::size_t mCellsPerChunk = 0;

        // Stage 4: residency-bounded cells pool (allocate on Loaded, free on leave Loaded).
        std::uint32_t mMaxBlocks = 0;
        std::uint32_t mNextUnusedBlock = 0;
        std::uint32_t mFreeBlockCount = 0;
        std::vector<std::uint32_t> mFreeBlockStack{}; // stores cell-offsets

        std::vector<SpatialChunk> mChunks{};
        std::vector<Cell> mCellsPool{};
        OverflowPool* mOverflowPool = nullptr;
    };

    class SlidingWindowStorage final {
      public:
        using Config = SlidingWindowConfig;

        void bindOverflowPool(OverflowPool* pool) noexcept {
            mOverflowPool = pool;
        }
        void init(const Config& config, const std::int32_t cellsPerChunkX,
                  const std::int32_t cellsPerChunkY);

        // NOTE (Stage 3): non-const tryGetChunk has a side-effect by design:
        // it may materialize (occupy) the slot for `coord` within the fixed window.
        // Stage 4 will split API into pure tryGetChunk() vs getOrCreateChunk() when origin shifts.
        [[nodiscard]] SpatialChunk* tryGetChunk(const ChunkCoord coord) noexcept;
        [[nodiscard]] const SpatialChunk* tryGetChunk(const ChunkCoord coord) const noexcept;

        template <typename Fn>
        bool forEachChunkInRange(const ChunkCoord minCoord, const ChunkCoord maxCoord,
                                 Fn&& fn) const;

        template <typename Fn>
        bool forEachChunkInRangeMut(const ChunkCoord minCoord, const ChunkCoord maxCoord,
                                    Fn&& fn);

        [[nodiscard]] bool setChunkState(const ChunkCoord coord, const ResidencyState state);
        [[nodiscard]] ResidencyState chunkState(const ChunkCoord coord) const noexcept;
        [[nodiscard]] Cell* cellsForChunk(SpatialChunk& chunk) noexcept;
        [[nodiscard]] const Cell* cellsForChunk(const SpatialChunk& chunk) const noexcept;

        void setWindowOrigin(const ChunkCoord origin) noexcept {
            if (origin.x == mOrigin.x && origin.y == mOrigin.y) {
                return;
            }

            assert(mShiftScratch.size() == mSlots.size() &&
                   "SlidingWindowStorage: shift scratch size mismatch");

            for (Slot& slot : mShiftScratch) {
                slot.coord = ChunkCoord{0, 0};
                slot.chunk.cellsBlockIndex = kInvalidCellsBlockIndex;
                detail::resetChunkOverlapCount(slot.chunk);
                slot.chunk.state = ResidencyState::Unloaded;
                slot.occupied = false;
            }

            for (Slot& slot : mSlots) {
                if (!slot.occupied) {
                    continue;
                }

                if (!isInWindow(slot.coord)) {
#if !defined(NDEBUG)
                    assert(false && "SlidingWindowStorage::setWindowOrigin: slot coord mismatch");
#else
                    LOG_PANIC(core::log::cat::ECS,
                              "SlidingWindowStorage::setWindowOrigin: slot coord mismatch");
#endif
                }

                if (!isInWindowForOrigin(slot.coord, origin)) {
                    resetSlot(slot);
                    continue;
                }

                const std::size_t dstIndex = slotIndexForOrigin(slot.coord, origin);
                Slot& dst = mShiftScratch[dstIndex];

#if !defined(NDEBUG)
                assert(!dst.occupied &&
                       "SlidingWindowStorage::setWindowOrigin: slot collision in shift");
#else
                if (dst.occupied) {
                    LOG_PANIC(core::log::cat::ECS,
                              "SlidingWindowStorage::setWindowOrigin: slot collision in shift");
                }
#endif
                transferSlot(dst, slot);
            }

            mSlots.swap(mShiftScratch);
            mOrigin = origin;

            const std::int64_t maxX64 = static_cast<std::int64_t>(mOrigin.x) + mWidth - 1;
            const std::int64_t maxY64 = static_cast<std::int64_t>(mOrigin.y) + mHeight - 1;

#if !defined(NDEBUG)
            assert(maxX64 >= std::numeric_limits<std::int32_t>::min() &&
                   maxX64 <= std::numeric_limits<std::int32_t>::max() &&
                   "SlidingWindowStorage: maxXInclusive overflow");
            assert(maxY64 >= std::numeric_limits<std::int32_t>::min() &&
                   maxY64 <= std::numeric_limits<std::int32_t>::max() &&
                   "SlidingWindowStorage: maxYInclusive overflow");
#else
            if (maxX64 < std::numeric_limits<std::int32_t>::min() ||
                maxX64 > std::numeric_limits<std::int32_t>::max() ||
                maxY64 < std::numeric_limits<std::int32_t>::min() ||
                maxY64 > std::numeric_limits<std::int32_t>::max()) {
                LOG_PANIC(core::log::cat::ECS,
                          "SlidingWindowStorage: max inclusive bounds overflow");
            }
#endif
            mMaxXInclusive = static_cast<std::int32_t>(maxX64);
            mMaxYInclusive = static_cast<std::int32_t>(maxY64);
        }

        [[nodiscard]] ChunkCoord origin() const noexcept {
            return mOrigin;
        }

        [[nodiscard]] std::int32_t width() const noexcept {
            return mWidth;
        }

        [[nodiscard]] std::int32_t height() const noexcept {
            return mHeight;
        }

      private:
        struct Slot final {
            ChunkCoord coord{0, 0};
            SpatialChunk chunk{};
            bool occupied = false;
        };

        [[nodiscard]] bool isInWindow(const ChunkCoord coord) const noexcept;
        [[nodiscard]] bool isInWindowForOrigin(const ChunkCoord coord,
                                               const ChunkCoord origin) const noexcept;
        [[nodiscard]] std::size_t slotIndex(const ChunkCoord coord) const noexcept;
        [[nodiscard]] std::size_t slotIndexForOrigin(const ChunkCoord coord,
                                                     const ChunkCoord origin) const noexcept;
        [[nodiscard]] Slot* ensureSlot(const ChunkCoord coord) noexcept;
        [[nodiscard]] const Slot* tryGetSlot(const ChunkCoord coord) const noexcept;

        [[nodiscard]] std::uint32_t allocateCellsBlock();
        void releaseCellsBlock(const std::uint32_t cellsBlockIndex) noexcept;

        void clearSlot(Slot& slot) noexcept;
        void clearChunkDataInSlot(Slot& slot) noexcept;
        void resetSlot(Slot& slot) noexcept;
        void transferSlot(Slot& dst, Slot& src) noexcept;

        ChunkCoord mOrigin{0, 0};
        std::int32_t mWidth = 0;
        std::int32_t mHeight = 0;

        // Precomputed inclusive bounds (avoid signed overflow in hot-path clamps).
        std::int32_t mMaxXInclusive = 0;
        std::int32_t mMaxYInclusive = 0;

        // Cells per chunk (precomputed once; avoids divisions).
        std::size_t mCellsPerChunk = 0;

        // Stage 4: residency-bounded cells pool.
        std::uint32_t mMaxBlocks = 0;
        std::uint32_t mNextUnusedBlock = 0;
        std::uint32_t mFreeBlockCount = 0;
        std::vector<std::uint32_t> mFreeBlockStack{}; // stores cell-offsets

        std::vector<Slot> mSlots{};
        std::vector<Slot> mShiftScratch{};
        std::vector<Cell> mCellsPool{};
        OverflowPool* mOverflowPool = nullptr;
    };

    template <typename Storage, typename BoundsT> class SpatialIndexV2 final {
      public:
        using StorageType = Storage;
        using BoundsType = BoundsT;

        SpatialIndexV2() = default;

        void init(const SpatialIndexConfig& config, const typename Storage::Config& storageConfig);
        void prewarm(const std::uint32_t maxEntityId);

        [[nodiscard]] WriteResult registerEntity(const EntityId32 id, const BoundsT& bounds);
        [[nodiscard]] WriteResult updateEntity(const EntityId32 id, const BoundsT& newBounds);
        [[nodiscard]] bool unregisterEntity(const EntityId32 id);

        // queryFast может truncate: если return == out.size(), результаты могли быть усечены.
        [[nodiscard]] std::size_t queryFast(const BoundsT& area, std::span<EntityId32> out) const;
        [[nodiscard]] std::size_t queryDeterministic(const BoundsT& area,
                                                     std::span<EntityId32> out) const;
        void collectEntityIdsInChunk(const ChunkCoord coord,
                                     std::vector<EntityId32>& outIdsScratch) const;

        [[nodiscard]] bool setChunkState(const ChunkCoord coord, const ResidencyState state) {
            return mStorage.setChunkState(coord, state);
        }

        void setWindowOrigin(const ChunkCoord origin) noexcept
            requires requires(Storage& storage, const ChunkCoord coord) {
                storage.setWindowOrigin(coord);
            }
        {
            mStorage.setWindowOrigin(origin);
        }

        [[nodiscard]] ChunkCoord windowOrigin() const noexcept {
            return mStorage.origin();
        }

        [[nodiscard]] std::int32_t windowWidth() const noexcept {
            return mStorage.width();
        }

        [[nodiscard]] std::int32_t windowHeight() const noexcept {
            return mStorage.height();
        }

        [[nodiscard]] ResidencyState chunkState(const ChunkCoord coord) const noexcept {
            return mStorage.chunkState(coord);
        }

        [[nodiscard]] std::int32_t chunkSizeWorld() const noexcept {
            return mConfig.chunkSizeWorld;
        }

        [[nodiscard]] std::int32_t cellSizeWorld() const noexcept {
            return mConfig.cellSizeWorld;
        }

        // Frame-level marks maintenance: single entry point for read-phase barrier.
        // Call once before any queries in a given update step.
        // Cheap no-op when no maintenance is required (single branch on bool flag).
        void beginFrameRead() const noexcept {
            if (mMarksClearRequired) {
                clearMarksTable();
            }
        }

        // Low-level marks API (prefer beginFrameRead() in normal usage).
        [[nodiscard]] bool marksClearRequired() const noexcept {
            return mMarksClearRequired;
        }

        // Global count of registered entities (accepted in write-path).
        [[nodiscard]] std::uint32_t activeEntityCount() const noexcept {
            return mActiveEntityCount;
        }

        void clearMarksTable() const noexcept;

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        void debugForceMarksMaintenanceRequired() noexcept {
            mMarksClearRequired = true;
        }

        struct DebugQueryStats final {
            std::uint32_t chunksVisited = 0;
            std::uint32_t chunksLoadedVisited = 0;
            std::uint32_t chunksSkippedNonLoaded = 0;
            std::uint32_t cellVisits = 0;
            std::uint32_t overflowCellVisits = 0;
            std::uint32_t overflowEntitiesVisited = 0;
            std::uint32_t entriesScanned = 0;
            std::uint32_t uniqueAdded = 0;
            std::uint32_t dupHits = 0;
            std::uint32_t outTruncated = 0;
            std::int32_t chunkMinX = 0;
            std::int32_t chunkMinY = 0;
            std::int32_t chunkMaxX = -1;
            std::int32_t chunkMaxY = -1;
            std::int32_t cellMinX = 0;
            std::int32_t cellMinY = 0;
            std::int32_t cellMaxX = -1;
            std::int32_t cellMaxY = -1;
        };

        struct DebugCellHealth final {
            std::uint32_t loaded = 0;
            std::uint32_t totalCells = 0;
            std::uint32_t maxCellLen = 0;
            std::uint32_t sumCellLen = 0;
            std::uint32_t dupApprox = 0;
        };

        struct DebugStats final {
            DebugQueryStats lastQuery{};
            std::uint32_t worstCellDensity = 0;
            std::uint32_t dupInsertDetected = 0;
            OverflowPool::DebugStats overflow{};
        };

        [[nodiscard]] const DebugQueryStats& debugLastQueryStatsRef() const noexcept {
            return mDebugLastQueryStats;
        }

        [[nodiscard]] std::uint32_t debugDupInsertDetected() const noexcept {
            return mDebugDupInsertDetected;
        }

        [[nodiscard]] DebugCellHealth
        debugCellHealthForChunk(const ChunkCoord coord) const noexcept;

        [[nodiscard]] bool debugWasInLastQuery(const EntityId32 id) const noexcept {
            return (id < mMarks.size()) && (mMarks[id] == mQueryStamp);
        }

        [[nodiscard]] DebugStats debugStats() const noexcept;
#endif

      private:
        struct CellRange final {
            std::int32_t minX = 0;
            std::int32_t minY = 0;
            std::int32_t maxX = -1;
            std::int32_t maxY = -1;

            [[nodiscard]] bool empty() const noexcept {
                return minX > maxX || minY > maxY;
            }
        };

        struct EntityRecord final {
            BoundsT bounds{};
            bool registered = false;
        };

        [[nodiscard]] bool validateConfig(const SpatialIndexConfig& config) const;
        void prepareStorage(const typename Storage::Config& storageConfig);
        void prewarmEntityCapacity(const std::uint32_t maxEntityId);
        void prewarmMarksCapacity(const std::uint32_t maxEntityId);
        void ensureEntityStorage(const EntityId32 id);
        void ensureMarksStorage(const EntityId32 id);

        [[nodiscard]] CellRange computeCellRange(const BoundsT& bounds) const noexcept;
        [[nodiscard]] CellRange computeChunkRange(const BoundsT& bounds) const noexcept;

        [[nodiscard]] bool canWriteRange(const CellRange& chunkRange) const;
        [[nodiscard]] WriteResult writeToChunk(SpatialChunk& chunk, const ChunkCoord coord,
                                       const CellRange& cellRange, const EntityId32 id);
        void removeFromChunk(SpatialChunk& chunk, const ChunkCoord coord,
                             const CellRange& cellRange, const EntityId32 id) noexcept;

        void appendToCell(Cell& cell, const EntityId32 id);
        void removeFromCell(Cell& cell, const EntityId32 id) noexcept;

        [[nodiscard]] bool beginQueryStamp() const;
        void handleMarksWrap() const;
        void handleNonLoadedChunk(const ChunkCoord coord) const;
        [[noreturn]] void handleOverflowExhausted() const;
        void handleOutputOverflow() const;

        template <typename ChunkT, typename Fn>
        bool forEachCellInRange(ChunkT& chunk, const ChunkCoord coord, const CellRange& cellRange,
                                Fn&& fn);

        template <typename ChunkT, typename Fn>
        bool forEachCellInRange(const ChunkT& chunk, const ChunkCoord coord,
                                const CellRange& cellRange, Fn&& fn) const;

        template <typename Fn> bool forEachEntityInCell(const Cell& cell, Fn&& fn) const;

        [[nodiscard]] CellRange clampCellRangeToChunk(const CellRange& range,
                                                      const ChunkCoord coord) const noexcept;

        [[nodiscard]] Cell* cellsForChunk(SpatialChunk& chunk) noexcept;
        [[nodiscard]] const Cell* cellsForChunk(const SpatialChunk& chunk) const noexcept;

        SpatialIndexConfig mConfig{};
        Storage mStorage{};
        OverflowPool mOverflowPool{};
        std::vector<EntityRecord> mEntities{};
        mutable std::vector<std::uint32_t> mMarks{};
        mutable std::uint32_t mQueryStamp = 1;
        mutable bool mMarksClearRequired = false;

        std::int32_t mCellsPerChunkX = 0;
        std::int32_t mCellsPerChunkY = 0;
        std::size_t mCellsPerChunk = 0;

        // Number of entities accepted by registerEntity() and
        // not yet removed by unregisterEntity().
        // Invariant: incremented ONLY on successful register, decremented ONLY on successful
        // unregister. updateEntity() does NOT touch this counter.
        // NOTE: entity with empty bounds IS counted (registered but occupies zero cells).
        std::uint32_t mActiveEntityCount = 0;

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        mutable DebugQueryStats mDebugLastQueryStats{};
        std::uint32_t mDebugWorstCellDensity = 0;
        std::uint32_t mDebugDupInsertDetected = 0;
#endif
    };

    using SpatialIndexV2Flat = SpatialIndexV2<FlatStorage, Aabb2>;
    using SpatialIndexV2Sliding = SpatialIndexV2<SlidingWindowStorage, Aabb2>;
    using SpatialIndexV2Titan = SpatialIndexV2<FlatStorage, Aabb2i>;

    // ---- Inline/template implementations ------------------------------------------------------

    inline void OverflowPool::init(const OverflowConfig& cfg) {
        mNodeCapacity = cfg.nodeCapacity;
        mMaxNodes = cfg.maxNodes;

        if (mNodeCapacity == 0 || mMaxNodes == 0) {
            mEntries.clear();
            mNext.clear();
            mCounts.clear();
            mTailByHead.clear();
            mFreeStack.clear();
            mFreeCount = 0;
            mNextUnusedIndex = 0;
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
            mPeakUsedNodes = 0;
#endif
            return;
        }

        mEntries.resize(static_cast<std::size_t>(mMaxNodes) * mNodeCapacity, 0u);
        mNext.assign(mMaxNodes, 0u);
        mCounts.assign(mMaxNodes, 0u);
        // Расширяем до size_t ДО сложения — mMaxNodes == UINT32_MAX не обернётся в 0.
        mTailByHead.assign(static_cast<std::size_t>(mMaxNodes) + 1u, 0u);

        mFreeStack.resize(mMaxNodes);
        mFreeCount = 0;
        mNextUnusedIndex = 0;
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        mPeakUsedNodes = 0;
#endif
    }

    inline std::uint32_t OverflowPool::allocateNode() {
#if !defined(NDEBUG)
        assert(mFreeCount <= mMaxNodes && "OverflowPool: freeCount corrupted");
#else
        if (mFreeCount > mMaxNodes) [[unlikely]] {
            LOG_PANIC(core::log::cat::ECS, "OverflowPool: freeCount corrupted");
        }
#endif

        if (mFreeCount > 0u) {
            const std::uint32_t handle = mFreeStack[--mFreeCount];
#if !defined(NDEBUG)
            assert(handle != 0u && "OverflowPool: invalid handle in free-stack");
#else
            if (handle == 0u) [[unlikely]] {
                LOG_PANIC(core::log::cat::ECS, "OverflowPool: invalid handle in free-stack");
            }
#endif
            const std::uint32_t idx = nodeIndex(handle);
            mCounts[idx] = 0;
            mNext[idx] = 0;

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
            const std::uint32_t used = mNextUnusedIndex - mFreeCount;
            if (used > mPeakUsedNodes) {
                mPeakUsedNodes = used;
            }
#endif
            return handle;
        }

        if (mNextUnusedIndex >= mMaxNodes) {
            return 0;
        }

        const std::uint32_t newIndex = mNextUnusedIndex++;
        mCounts[newIndex] = 0;
        mNext[newIndex] = 0;

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        const std::uint32_t used = mNextUnusedIndex - mFreeCount;
        if (used > mPeakUsedNodes) {
            mPeakUsedNodes = used;
        }
#endif
        return handleFromIndex(newIndex);
    }

    inline void OverflowPool::releaseNode(const std::uint32_t handle) noexcept {
        if (handle == 0) {
            return;
        }

#if !defined(NDEBUG)
        assert(mFreeCount < mMaxNodes && "OverflowPool: free-stack overflow (double free?)");
#else
        if (mFreeCount >= mMaxNodes) [[unlikely]] {
            LOG_PANIC(core::log::cat::ECS, "OverflowPool: free-stack overflow (double free?)");
        }
#endif

        const std::uint32_t idx = nodeIndex(handle);
        mCounts[idx] = 0;
        mNext[idx] = 0;
        mFreeStack[mFreeCount++] = handle;
    }

    inline bool OverflowPool::append(std::uint32_t& headHandle, const EntityId32 id) {
        if (mNodeCapacity == 0 || mMaxNodes == 0) {
            return false;
        }

        if (headHandle == 0u) {
            headHandle = allocateNode();
            if (headHandle == 0u) {
                return false;
            }
            mTailByHead[headHandle] = headHandle;
        }

        const std::uint32_t tailHandle = mTailByHead[headHandle];

#if !defined(NDEBUG)
        assert(tailHandle != 0u &&
               "OverflowPool: tail invariant violated (headHandle valid but tailByHead==0)");
#else
        if (tailHandle == 0u) [[unlikely]] {
            LOG_PANIC(core::log::cat::ECS,
                      "OverflowPool: tail invariant violated (headHandle valid but tailByHead==0)");
        }
#endif

        std::uint32_t idx = nodeIndex(tailHandle);

        if (mCounts[idx] >= mNodeCapacity) {
            const std::uint32_t newHandle = allocateNode();
            if (newHandle == 0u) {
                return false;
            }
            mNext[idx] = newHandle;
            mTailByHead[headHandle] = newHandle;
            idx = nodeIndex(newHandle);
        }

        const std::size_t offset = entryOffset(idx);
        mEntries[offset + mCounts[idx]] = id;
        ++mCounts[idx];
        return true;
    }

    inline bool OverflowPool::remove(std::uint32_t& headHandle, const EntityId32 id) noexcept {
        if (headHandle == 0u) {
            return false;
        }

        const std::uint32_t tailHandle = mTailByHead[headHandle];

#if !defined(NDEBUG)
        assert(tailHandle != 0u &&
               "OverflowPool: tail invariant violated (headHandle valid but tailByHead==0)");
#else
        if (tailHandle == 0u) [[unlikely]] {
            LOG_PANIC(core::log::cat::ECS,
                      "OverflowPool: tail invariant violated (headHandle valid but tailByHead==0)");
        }
#endif

        std::uint32_t prevHandle = 0u;
        std::uint32_t handle = headHandle;

        while (handle != 0u) {
            const std::uint32_t idx = nodeIndex(handle);
            const std::size_t offset = entryOffset(idx);
            const std::uint32_t count = mCounts[idx];

            for (std::uint32_t i = 0; i < count; ++i) {
                if (mEntries[offset + i] != id) {
                    continue;
                }

                for (std::uint32_t j = i + 1u; j < count; ++j) {
                    mEntries[offset + j - 1u] = mEntries[offset + j];
                }
                mCounts[idx] = count - 1u;

                if (mCounts[idx] == 0u) {
                    const std::uint32_t nextHandle = mNext[idx];

                    if (prevHandle == 0u) {
                        // Removing head node.
                        const std::uint32_t oldHead = headHandle;
                        headHandle = nextHandle;

#if !defined(NDEBUG)
                        assert(!(headHandle != 0u && tailHandle == oldHead) &&
                               "OverflowPool: tail invariant violated "
                               "(tail==oldHead but chain continues)");
#else
                        if (headHandle != 0u && tailHandle == oldHead) [[unlikely]] {
                            LOG_PANIC(core::log::cat::ECS, "OverflowPool: tail invariant violated "
                                                           "(tail==oldHead but chain continues)");
                        }
#endif

                        if (headHandle != 0u) {
                            // Tail for new head is the old tail.
                            mTailByHead[headHandle] = tailHandle;
                        }
                        mTailByHead[oldHead] = 0u;
                    } else {
                        // Removing non-head node: splice out.
                        mNext[nodeIndex(prevHandle)] = nextHandle;

                        // If we removed the tail node, tail becomes prevHandle.
                        if (handle == tailHandle) {
#if !defined(NDEBUG)
                            assert(headHandle != 0u &&
                                   "OverflowPool: removing tail from empty head?");
                            assert(prevHandle != 0u && "OverflowPool: tail remove without prev?");
#else
                            if (headHandle == 0u || prevHandle == 0u) [[unlikely]] {
                                LOG_PANIC(core::log::cat::ECS,
                                          "OverflowPool: tail remove invariant violated");
                            }
#endif
                            mTailByHead[headHandle] = prevHandle;
                        }
                    }

                    releaseNode(handle);
                }

#if !defined(NDEBUG)
                if (headHandle != 0u) {
                    assert(mTailByHead[headHandle] != 0u &&
                           "OverflowPool: tail became 0 after remove");
                }
#endif
                return true;
            }

            prevHandle = handle;
            handle = mNext[idx];
        }

        return false;
    }

    inline void OverflowPool::releaseChain(std::uint32_t& headHandle) noexcept {
        const std::uint32_t oldHead = headHandle;

        std::uint32_t handle = headHandle;
        while (handle != 0u) {
            const std::uint32_t idx = nodeIndex(handle);
            const std::uint32_t nextHandle = mNext[idx];
            releaseNode(handle);
            handle = nextHandle;
        }

        if (oldHead != 0u) {
            mTailByHead[oldHead] = 0u;
        }
        headHandle = 0u;
    }

    template <typename Fn>
    inline bool OverflowPool::forEach(const std::uint32_t headHandle, Fn&& fn) const {
        std::uint32_t handle = headHandle;
        while (handle != 0u) {
            const std::uint32_t idx = nodeIndex(handle);
            const std::size_t offset = entryOffset(idx);
            const std::uint32_t count = mCounts[idx];
            for (std::uint32_t i = 0; i < count; ++i) {
                if (!fn(mEntries[offset + i])) {
                    return false;
                }
            }
            handle = mNext[idx];
        }
        return true;
    }

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
    inline OverflowPool::DebugStats OverflowPool::debugStats() const noexcept {
        return DebugStats{.nodeCapacity = mNodeCapacity,
                          .maxNodes = mMaxNodes,
                          .peakUsedNodes = mPeakUsedNodes,
                          .freeDepth = mFreeCount};
    }

    inline std::uint32_t OverflowPool::totalCount(const std::uint32_t headHandle) const noexcept {
        std::uint32_t total = 0;
        std::uint32_t handle = headHandle;
        while (handle != 0u) {
            const std::uint32_t idx = nodeIndex(handle);
            total += mCounts[idx];
            handle = mNext[idx];
        }
        return total;
    }
#endif

    inline std::uint32_t FlatStorage::allocateCellsBlock() {
#if !defined(NDEBUG)
        assert(mFreeBlockCount <= mMaxBlocks && "FlatStorage: freeBlockCount corrupted");
#else
        if (mFreeBlockCount > mMaxBlocks) [[unlikely]] {
            LOG_PANIC(core::log::cat::ECS, "FlatStorage: freeBlockCount corrupted");
        }
#endif

        if (mFreeBlockCount > 0u) {
            const std::uint32_t cellsOffset = mFreeBlockStack[--mFreeBlockCount];
            return cellsOffset;
        }

        if (mNextUnusedBlock >= mMaxBlocks) {
            return kInvalidCellsBlockIndex;
        }

        const std::uint64_t offset64 = static_cast<std::uint64_t>(mNextUnusedBlock) *
                                       static_cast<std::uint64_t>(mCellsPerChunk);
        ++mNextUnusedBlock;

#if !defined(NDEBUG)
        assert(offset64 <= std::numeric_limits<std::uint32_t>::max() &&
               "FlatStorage: cells block offset overflow");
#else
        if (offset64 > std::numeric_limits<std::uint32_t>::max()) [[unlikely]] {
            LOG_PANIC(core::log::cat::ECS, "FlatStorage: cells block offset overflow");
        }
#endif

        return static_cast<std::uint32_t>(offset64);
    }

    inline void FlatStorage::releaseCellsBlock(const std::uint32_t cellsBlockIndex) noexcept {
        if (cellsBlockIndex == kInvalidCellsBlockIndex) {
            return;
        }

#if !defined(NDEBUG)
        assert(mFreeBlockCount < mMaxBlocks &&
               "FlatStorage: free-block stack overflow (double free?)");
#else
        if (mFreeBlockCount >= mMaxBlocks) [[unlikely]] {
            LOG_PANIC(core::log::cat::ECS, "FlatStorage: free-block stack overflow (double free?)");
        }
#endif
        mFreeBlockStack[mFreeBlockCount++] = cellsBlockIndex;
    }

    inline void FlatStorage::init(const Config& config, const std::int32_t cellsPerChunkX,
                                  const std::int32_t cellsPerChunkY) {
#if !defined(NDEBUG)
        assert(config.width > 0 && config.height > 0 && "FlatStorage: invalid dimensions");
#else
        if (config.width <= 0 || config.height <= 0) {
            LOG_PANIC(core::log::cat::ECS, "FlatStorage: invalid dimensions");
        }
#endif

        mOrigin = config.origin;
        mWidth = config.width;
        mHeight = config.height;

        // Precompute inclusive bounds using int64 to avoid signed overflow UB.
        {
            const std::int64_t maxX64 = static_cast<std::int64_t>(mOrigin.x) + mWidth - 1;
            const std::int64_t maxY64 = static_cast<std::int64_t>(mOrigin.y) + mHeight - 1;

#if !defined(NDEBUG)
            assert(maxX64 >= std::numeric_limits<std::int32_t>::min() &&
                   maxX64 <= std::numeric_limits<std::int32_t>::max() &&
                   "FlatStorage: maxXInclusive overflow");
            assert(maxY64 >= std::numeric_limits<std::int32_t>::min() &&
                   maxY64 <= std::numeric_limits<std::int32_t>::max() &&
                   "FlatStorage: maxYInclusive overflow");
#else
            if (maxX64 < std::numeric_limits<std::int32_t>::min() ||
                maxX64 > std::numeric_limits<std::int32_t>::max() ||
                maxY64 < std::numeric_limits<std::int32_t>::min() ||
                maxY64 > std::numeric_limits<std::int32_t>::max()) {
                LOG_PANIC(core::log::cat::ECS, "FlatStorage: max inclusive bounds overflow");
            }
#endif
            mMaxXInclusive = static_cast<std::int32_t>(maxX64);
            mMaxYInclusive = static_cast<std::int32_t>(maxY64);
        }

        const std::size_t widthSz = static_cast<std::size_t>(mWidth);
        const std::size_t heightSz = static_cast<std::size_t>(mHeight);

#if !defined(NDEBUG)
        assert(widthSz == 0 || heightSz <= (std::numeric_limits<std::size_t>::max() / widthSz));
#else
        if (widthSz != 0 && heightSz > (std::numeric_limits<std::size_t>::max() / widthSz)) {
            LOG_PANIC(core::log::cat::ECS, "FlatStorage: chunks array size overflow");
        }
#endif

        const std::size_t count = widthSz * heightSz;

#if !defined(NDEBUG)
        assert(count <= std::numeric_limits<std::uint32_t>::max() &&
               "FlatStorage: chunk count exceeds uint32 range (unsupported)");
#else
        if (count > std::numeric_limits<std::uint32_t>::max()) {
            LOG_PANIC(core::log::cat::ECS, "FlatStorage: chunk count exceeds uint32 range");
        }
#endif

        mChunks.clear();
        mChunks.resize(count);

        const std::size_t cellsCount = static_cast<std::size_t>(cellsPerChunkX) * cellsPerChunkY;
        mCellsPerChunk = cellsCount;

#if !defined(NDEBUG)
        assert(cellsCount > 0 && "FlatStorage: cellsPerChunk must be > 0");
#else
        if (cellsCount == 0) {
            LOG_PANIC(core::log::cat::ECS, "FlatStorage: cellsPerChunk must be > 0");
        }
#endif

        const std::size_t maxBlocksSz = (config.maxResidentChunks != 0u)
                                            ? static_cast<std::size_t>(config.maxResidentChunks)
                                            : count;

#if !defined(NDEBUG)
        assert(maxBlocksSz > 0 && "FlatStorage: maxBlocks must be > 0");
        assert(maxBlocksSz <= count && "FlatStorage: maxResidentChunks exceeds total chunk count");
#else
        if (maxBlocksSz == 0 || maxBlocksSz > count) {
            LOG_PANIC(core::log::cat::ECS,
                      "FlatStorage: invalid maxResidentChunks (maxBlocks={}, total={})",
                      maxBlocksSz, count);
        }
#endif

#if !defined(NDEBUG)
        assert(maxBlocksSz <= std::numeric_limits<std::uint32_t>::max() &&
               "FlatStorage: maxBlocks exceeds uint32 range");
#else
        if (maxBlocksSz > std::numeric_limits<std::uint32_t>::max()) {
            LOG_PANIC(core::log::cat::ECS, "FlatStorage: maxBlocks exceeds uint32 range");
        }
#endif

        mMaxBlocks = static_cast<std::uint32_t>(maxBlocksSz);
        mNextUnusedBlock = 0;
        mFreeBlockCount = 0;
        mFreeBlockStack.resize(mMaxBlocks);

#if !defined(NDEBUG)
        assert(maxBlocksSz <= (std::numeric_limits<std::size_t>::max() / cellsCount));
#else
        if (maxBlocksSz > (std::numeric_limits<std::size_t>::max() / cellsCount)) {
            LOG_PANIC(core::log::cat::ECS, "FlatStorage: cells pool size overflow");
        }
#endif

        // Stage 4 pool: capacity == max resident chunks (not total chunks).
        mCellsPool.assign(maxBlocksSz * cellsCount, Cell{});

        for (SpatialChunk& chunk : mChunks) {
            chunk.cellsBlockIndex = kInvalidCellsBlockIndex;
            detail::resetChunkOverlapCount(chunk);
            chunk.state = ResidencyState::Unloaded;
        }
    }

    inline bool FlatStorage::isInBounds(const ChunkCoord coord) const noexcept {
        const std::int32_t dx = coord.x - mOrigin.x;
        const std::int32_t dy = coord.y - mOrigin.y;
        return dx >= 0 && dx < mWidth && dy >= 0 && dy < mHeight;
    }

    inline std::size_t FlatStorage::indexOf(const ChunkCoord coord) const noexcept {
        const std::int64_t localX = static_cast<std::int64_t>(coord.x) - mOrigin.x;
        const std::int64_t localY = static_cast<std::int64_t>(coord.y) - mOrigin.y;
        const std::int64_t idx = localY * mWidth + localX;
        return static_cast<std::size_t>(idx);
    }

    inline SpatialChunk* FlatStorage::tryGetChunk(const ChunkCoord coord) noexcept {
        if (!isInBounds(coord)) {
            return nullptr;
        }
        return &mChunks[indexOf(coord)];
    }

    inline const SpatialChunk* FlatStorage::tryGetChunk(const ChunkCoord coord) const noexcept {
        if (!isInBounds(coord)) {
            return nullptr;
        }
        return &mChunks[indexOf(coord)];
    }

    inline Cell* FlatStorage::cellsForChunk(SpatialChunk& chunk) noexcept {
        if (chunk.cellsBlockIndex == kInvalidCellsBlockIndex) {
            return nullptr;
        }
        return mCellsPool.data() + chunk.cellsBlockIndex;
    }

    inline const Cell* FlatStorage::cellsForChunk(const SpatialChunk& chunk) const noexcept {
        if (chunk.cellsBlockIndex == kInvalidCellsBlockIndex) {
            return nullptr;
        }
        return mCellsPool.data() + chunk.cellsBlockIndex;
    }

    inline void FlatStorage::clearChunkData(SpatialChunk& chunk) noexcept {
        Cell* cells = cellsForChunk(chunk);
        detail::clearChunkCells(cells, mCellsPerChunk, mOverflowPool);
        detail::resetChunkOverlapCount(chunk);
    }

    template <typename Fn>
    inline bool FlatStorage::forEachChunkInRange(const ChunkCoord minCoord,
                                                 const ChunkCoord maxCoord, Fn&& fn) const {
        if (mChunks.empty()) {
            return true;
        }

        const std::int32_t clampedMinX = std::max(minCoord.x, mOrigin.x);
        const std::int32_t clampedMinY = std::max(minCoord.y, mOrigin.y);
        const std::int32_t clampedMaxX = std::min(maxCoord.x, mMaxXInclusive);
        const std::int32_t clampedMaxY = std::min(maxCoord.y, mMaxYInclusive);

        if (clampedMinX > clampedMaxX || clampedMinY > clampedMaxY) {
            return true;
        }

        for (std::int32_t y = clampedMinY; y <= clampedMaxY; ++y) {
            for (std::int32_t x = clampedMinX; x <= clampedMaxX; ++x) {
                const ChunkCoord coord{x, y};
                const SpatialChunk& chunk = mChunks[indexOf(coord)];
                if (!fn(coord, chunk)) {
                    return false;
                }
            }
        }
        return true;
    }

    template <typename Fn>
    inline bool FlatStorage::forEachChunkInRangeMut(const ChunkCoord minCoord,
                                                    const ChunkCoord maxCoord, Fn&& fn) {
        if (mChunks.empty()) {
            return true;
        }

        const std::int32_t clampedMinX = std::max(minCoord.x, mOrigin.x);
        const std::int32_t clampedMinY = std::max(minCoord.y, mOrigin.y);
        const std::int32_t clampedMaxX = std::min(maxCoord.x, mMaxXInclusive);
        const std::int32_t clampedMaxY = std::min(maxCoord.y, mMaxYInclusive);

        if (clampedMinX > clampedMaxX || clampedMinY > clampedMaxY) {
            return true;
        }

        for (std::int32_t y = clampedMinY; y <= clampedMaxY; ++y) {
            const std::int64_t baseIdx =
                static_cast<std::int64_t>(y - mOrigin.y) * mWidth - mOrigin.x;
            for (std::int32_t x = clampedMinX; x <= clampedMaxX; ++x) {
                const std::size_t idx =
                    static_cast<std::size_t>(baseIdx + static_cast<std::int64_t>(x));
                SpatialChunk& chunk = mChunks[idx];
                if (!fn(ChunkCoord{x, y}, chunk)) {
                    return false;
                }
            }
        }

        return true;
    }

    inline bool FlatStorage::setChunkState(const ChunkCoord coord, const ResidencyState state) {
        SpatialChunk* chunk = tryGetChunk(coord);
        if (chunk == nullptr) {
            return false;
        }

        const ResidencyState prev = chunk->state;
        if (prev == state) {
            return true;
        }

        // Strict model:
        //  - Only Loaded chunks may hold cell data (and therefore must own an allocated block).
        //  - Leaving Loaded => clear + free block immediately.
        //  - Entering Loaded => allocate block (fail-fast if capacity exhausted).
        if (prev == ResidencyState::Loaded && state != ResidencyState::Loaded) {
#if !defined(NDEBUG)
            assert(detail::chunkOverlapCountIsZero(*chunk) &&
                   "FlatStorage: unload with active entities (coverage != 0)");
#elif defined(SFML1_PROFILE)
            if (!detail::chunkOverlapCountIsZero(*chunk)) [[unlikely]] {
                LOG_PANIC(core::log::cat::ECS,
                          "FlatStorage: unload with active entities (coverage={})",
                          detail::chunkOverlapCountForLog(*chunk));
            }
#endif
            clearChunkData(*chunk);

#if !defined(NDEBUG)
            assert(chunk->cellsBlockIndex != kInvalidCellsBlockIndex &&
                   "FlatStorage: Loaded chunk must own a cells block");
#else
            if (chunk->cellsBlockIndex == kInvalidCellsBlockIndex) [[unlikely]] {
                LOG_PANIC(core::log::cat::ECS, "FlatStorage: Loaded chunk missing cells block");
            }
#endif
            releaseCellsBlock(chunk->cellsBlockIndex);
            chunk->cellsBlockIndex = kInvalidCellsBlockIndex;
        }

        if (prev != ResidencyState::Loaded && state == ResidencyState::Loaded) {
#if !defined(NDEBUG)
            assert(chunk->cellsBlockIndex == kInvalidCellsBlockIndex &&
                   "FlatStorage: non-Loaded chunk must not own a cells block");
#else
            if (chunk->cellsBlockIndex != kInvalidCellsBlockIndex) [[unlikely]] {
                LOG_PANIC(core::log::cat::ECS, "FlatStorage: non-Loaded chunk owns a cells block");
            }
#endif

            const std::uint32_t newBlock = allocateCellsBlock();
#if !defined(NDEBUG)
            assert(newBlock != kInvalidCellsBlockIndex &&
                   "FlatStorage: cells pool exhausted (increase maxResidentChunks)");
#else
            if (newBlock == kInvalidCellsBlockIndex) [[unlikely]] {
                LOG_PANIC(core::log::cat::ECS,
                          "FlatStorage: cells pool exhausted (increase maxResidentChunks)");
            }
#endif
            chunk->cellsBlockIndex = newBlock;

            // Freshly loaded chunk starts empty.
            clearChunkData(*chunk);
        }

        chunk->state = state;
        return true;
    }

    inline ResidencyState FlatStorage::chunkState(const ChunkCoord coord) const noexcept {
        const SpatialChunk* chunk = tryGetChunk(coord);
        if (chunk == nullptr) {
            return ResidencyState::Unloaded;
        }
        return chunk->state;
    }

    inline std::uint32_t SlidingWindowStorage::allocateCellsBlock() {
#if !defined(NDEBUG)
        assert(mFreeBlockCount <= mMaxBlocks && "SlidingWindowStorage: freeBlockCount corrupted");
#else
        if (mFreeBlockCount > mMaxBlocks) [[unlikely]] {
            LOG_PANIC(core::log::cat::ECS, "SlidingWindowStorage: freeBlockCount corrupted");
        }
#endif

        if (mFreeBlockCount > 0u) {
            const std::uint32_t cellsOffset = mFreeBlockStack[--mFreeBlockCount];
            return cellsOffset;
        }

        if (mNextUnusedBlock >= mMaxBlocks) {
            return kInvalidCellsBlockIndex;
        }

        const std::uint64_t offset64 = static_cast<std::uint64_t>(mNextUnusedBlock) *
                                       static_cast<std::uint64_t>(mCellsPerChunk);
        ++mNextUnusedBlock;

#if !defined(NDEBUG)
        assert(offset64 <= std::numeric_limits<std::uint32_t>::max() &&
               "SlidingWindowStorage: cells block offset overflow");
#else
        if (offset64 > std::numeric_limits<std::uint32_t>::max()) [[unlikely]] {
            LOG_PANIC(core::log::cat::ECS, "SlidingWindowStorage: cells block offset overflow");
        }
#endif

        return static_cast<std::uint32_t>(offset64);
    }

    inline void
    SlidingWindowStorage::releaseCellsBlock(const std::uint32_t cellsBlockIndex) noexcept {
        if (cellsBlockIndex == kInvalidCellsBlockIndex) {
            return;
        }

#if !defined(NDEBUG)
        assert(mFreeBlockCount < mMaxBlocks &&
               "SlidingWindowStorage: free-block stack overflow (double free?)");
#else
        if (mFreeBlockCount >= mMaxBlocks) [[unlikely]] {
            LOG_PANIC(core::log::cat::ECS,
                      "SlidingWindowStorage: free-block stack overflow (double free?)");
        }
#endif
        mFreeBlockStack[mFreeBlockCount++] = cellsBlockIndex;
    }

    inline void SlidingWindowStorage::init(const Config& config, const std::int32_t cellsPerChunkX,
                                           const std::int32_t cellsPerChunkY) {
#if !defined(NDEBUG)
        assert(config.width > 0 && config.height > 0 && "SlidingWindowStorage: invalid dimensions");
#else
        if (config.width <= 0 || config.height <= 0) {
            LOG_PANIC(core::log::cat::ECS, "SlidingWindowStorage: invalid dimensions");
        }
#endif

        mOrigin = config.origin;
        mWidth = config.width;
        mHeight = config.height;

        // Precompute inclusive bounds using int64 to avoid signed overflow UB.
        {
            const std::int64_t maxX64 = static_cast<std::int64_t>(mOrigin.x) + mWidth - 1;
            const std::int64_t maxY64 = static_cast<std::int64_t>(mOrigin.y) + mHeight - 1;

#if !defined(NDEBUG)
            assert(maxX64 >= std::numeric_limits<std::int32_t>::min() &&
                   maxX64 <= std::numeric_limits<std::int32_t>::max() &&
                   "SlidingWindowStorage: maxXInclusive overflow");
            assert(maxY64 >= std::numeric_limits<std::int32_t>::min() &&
                   maxY64 <= std::numeric_limits<std::int32_t>::max() &&
                   "SlidingWindowStorage: maxYInclusive overflow");
#else
            if (maxX64 < std::numeric_limits<std::int32_t>::min() ||
                maxX64 > std::numeric_limits<std::int32_t>::max() ||
                maxY64 < std::numeric_limits<std::int32_t>::min() ||
                maxY64 > std::numeric_limits<std::int32_t>::max()) {
                LOG_PANIC(core::log::cat::ECS,
                          "SlidingWindowStorage: max inclusive bounds overflow");
            }
#endif
            mMaxXInclusive = static_cast<std::int32_t>(maxX64);
            mMaxYInclusive = static_cast<std::int32_t>(maxY64);
        }

        const std::size_t widthSz = static_cast<std::size_t>(mWidth);
        const std::size_t heightSz = static_cast<std::size_t>(mHeight);

#if !defined(NDEBUG)
        assert(widthSz == 0 || heightSz <= (std::numeric_limits<std::size_t>::max() / widthSz));
#else
        if (widthSz != 0 && heightSz > (std::numeric_limits<std::size_t>::max() / widthSz)) {
            LOG_PANIC(core::log::cat::ECS, "SlidingWindowStorage: slots array size overflow");
        }
#endif

        const std::size_t count = widthSz * heightSz;

#if !defined(NDEBUG)
        assert(count <= std::numeric_limits<std::uint32_t>::max() &&
               "SlidingWindowStorage: slot count exceeds uint32 range (unsupported)");
#else
        if (count > std::numeric_limits<std::uint32_t>::max()) {
            LOG_PANIC(core::log::cat::ECS, "SlidingWindowStorage: slot count exceeds uint32 range");
        }
#endif

        mSlots.clear();
        mSlots.resize(count);
        mShiftScratch.clear();
        mShiftScratch.resize(count);

        const std::size_t cellsCount = static_cast<std::size_t>(cellsPerChunkX) * cellsPerChunkY;
        mCellsPerChunk = cellsCount;

#if !defined(NDEBUG)
        assert(cellsCount > 0 && "SlidingWindowStorage: cellsPerChunk must be > 0");
#else
        if (cellsCount == 0) {
            LOG_PANIC(core::log::cat::ECS, "SlidingWindowStorage: cellsPerChunk must be > 0");
        }
#endif

        const std::size_t maxBlocksSz = (config.maxResidentChunks != 0u)
                                            ? static_cast<std::size_t>(config.maxResidentChunks)
                                            : count;

#if !defined(NDEBUG)
        assert(maxBlocksSz > 0 && "SlidingWindowStorage: maxBlocks must be > 0");
        assert(maxBlocksSz <= count &&
               "SlidingWindowStorage: maxResidentChunks exceeds window size");
#else
        if (maxBlocksSz == 0 || maxBlocksSz > count) {
            LOG_PANIC(core::log::cat::ECS,
                      "SlidingWindowStorage: invalid maxResidentChunks (maxBlocks={}, window={})",
                      maxBlocksSz, count);
        }
#endif

#if !defined(NDEBUG)
        assert(maxBlocksSz <= std::numeric_limits<std::uint32_t>::max() &&
               "SlidingWindowStorage: maxBlocks exceeds uint32 range");
#else
        if (maxBlocksSz > std::numeric_limits<std::uint32_t>::max()) {
            LOG_PANIC(core::log::cat::ECS, "SlidingWindowStorage: maxBlocks exceeds uint32 range");
        }
#endif

        mMaxBlocks = static_cast<std::uint32_t>(maxBlocksSz);
        mNextUnusedBlock = 0;
        mFreeBlockCount = 0;
        mFreeBlockStack.resize(mMaxBlocks);

#if !defined(NDEBUG)
        assert(maxBlocksSz <= (std::numeric_limits<std::size_t>::max() / cellsCount));
#else
        if (maxBlocksSz > (std::numeric_limits<std::size_t>::max() / cellsCount)) {
            LOG_PANIC(core::log::cat::ECS, "SlidingWindowStorage: cells pool size overflow");
        }
#endif

        mCellsPool.assign(maxBlocksSz * cellsCount, Cell{});

        for (Slot& slot : mSlots) {
            slot.chunk.cellsBlockIndex = kInvalidCellsBlockIndex;
            detail::resetChunkOverlapCount(slot.chunk);
            slot.chunk.state = ResidencyState::Unloaded;
            slot.coord = ChunkCoord{0, 0};
            slot.occupied = false;
        }

        for (Slot& slot : mShiftScratch) {
            slot.chunk.cellsBlockIndex = kInvalidCellsBlockIndex;
            detail::resetChunkOverlapCount(slot.chunk);
            slot.chunk.state = ResidencyState::Unloaded;
            slot.coord = ChunkCoord{0, 0};
            slot.occupied = false;
        }
    }

    inline bool SlidingWindowStorage::isInWindow(const ChunkCoord coord) const noexcept {
        const std::int32_t dx = coord.x - mOrigin.x;
        const std::int32_t dy = coord.y - mOrigin.y;
        return dx >= 0 && dx < mWidth && dy >= 0 && dy < mHeight;
    }

    inline bool SlidingWindowStorage::isInWindowForOrigin(const ChunkCoord coord,
                                                          const ChunkCoord origin) const noexcept {
        const std::int32_t dx = coord.x - origin.x;
        const std::int32_t dy = coord.y - origin.y;
        return dx >= 0 && dx < mWidth && dy >= 0 && dy < mHeight;
    }

    inline std::size_t SlidingWindowStorage::slotIndex(const ChunkCoord coord) const noexcept {
        const std::int32_t localX = coord.x - mOrigin.x;
        const std::int32_t localY = coord.y - mOrigin.y;
        const std::int32_t wrapX = detail::euclideanMod(localX, mWidth);
        const std::int32_t wrapY = detail::euclideanMod(localY, mHeight);
        const std::int64_t idx = static_cast<std::int64_t>(wrapY) * mWidth + wrapX;
        return static_cast<std::size_t>(idx);
    }

    inline std::size_t
    SlidingWindowStorage::slotIndexForOrigin(const ChunkCoord coord,
                                             const ChunkCoord origin) const noexcept {
        const std::int32_t localX = coord.x - origin.x;
        const std::int32_t localY = coord.y - origin.y;
        const std::int32_t wrapX = detail::euclideanMod(localX, mWidth);
        const std::int32_t wrapY = detail::euclideanMod(localY, mHeight);
        const std::int64_t idx = static_cast<std::int64_t>(wrapY) * mWidth + wrapX;
        return static_cast<std::size_t>(idx);
    }

    inline void SlidingWindowStorage::clearChunkDataInSlot(Slot& slot) noexcept {
        Cell* cells = cellsForChunk(slot.chunk);
        detail::clearChunkCells(cells, mCellsPerChunk, mOverflowPool);
        detail::resetChunkOverlapCount(slot.chunk);
    }

    inline void SlidingWindowStorage::clearSlot(Slot& slot) noexcept {
        if (slot.chunk.state == ResidencyState::Loaded) {
#if !defined(NDEBUG)
            assert(detail::chunkOverlapCountIsZero(slot.chunk) &&
                   "SlidingWindowStorage: unload with active entities (coverage != 0)");
#elif defined(SFML1_PROFILE)
            if (!detail::chunkOverlapCountIsZero(slot.chunk)) [[unlikely]] {
                LOG_PANIC(core::log::cat::ECS,
                          "SlidingWindowStorage: unload with active entities (coverage={})",
                          detail::chunkOverlapCountForLog(slot.chunk));
            }
#endif
        }

        if (slot.chunk.cellsBlockIndex != kInvalidCellsBlockIndex) {
            clearChunkDataInSlot(slot);
            releaseCellsBlock(slot.chunk.cellsBlockIndex);
            slot.chunk.cellsBlockIndex = kInvalidCellsBlockIndex;
        }

        detail::resetChunkOverlapCount(slot.chunk);
        slot.chunk.state = ResidencyState::Unloaded;
        slot.occupied = false;
        slot.coord = ChunkCoord{0, 0};
    }

    inline void SlidingWindowStorage::resetSlot(Slot& slot) noexcept {
        clearSlot(slot);
    }

    inline void SlidingWindowStorage::transferSlot(Slot& dst, Slot& src) noexcept {
        dst.coord = src.coord;
        dst.chunk = src.chunk;
        dst.occupied = src.occupied;

        src.coord = ChunkCoord{0, 0};
        src.chunk.cellsBlockIndex = kInvalidCellsBlockIndex;
        detail::resetChunkOverlapCount(src.chunk);
        src.chunk.state = ResidencyState::Unloaded;
        src.occupied = false;
    }

    inline SlidingWindowStorage::Slot*
    SlidingWindowStorage::ensureSlot(const ChunkCoord coord) noexcept {
        if (!isInWindow(coord)) {
            return nullptr;
        }

        Slot& slot = mSlots[slotIndex(coord)];

        if (slot.occupied && (slot.coord.x != coord.x || slot.coord.y != coord.y)) {
            // With fixed origin (Stage 3), this is impossible-state.
#if !defined(NDEBUG)
            assert(false && "SlidingWindowStorage: slot coord mismatch (internal error)");
#else
            LOG_PANIC(core::log::cat::ECS,
                      "SlidingWindowStorage: slot coord mismatch (internal error)");
#endif
        }

        if (!slot.occupied) {
            slot.coord = coord;
            slot.occupied = true;
            slot.chunk.state = ResidencyState::Unloaded;
            detail::resetChunkOverlapCount(slot.chunk);
            slot.chunk.cellsBlockIndex = kInvalidCellsBlockIndex;
        }

        return &slot;
    }

    inline const SlidingWindowStorage::Slot*
    SlidingWindowStorage::tryGetSlot(const ChunkCoord coord) const noexcept {
        if (!isInWindow(coord)) {
            return nullptr;
        }

        const Slot& slot = mSlots[slotIndex(coord)];
        if (!slot.occupied || slot.coord.x != coord.x || slot.coord.y != coord.y) {
            return nullptr;
        }

        return &slot;
    }

    inline SpatialChunk* SlidingWindowStorage::tryGetChunk(const ChunkCoord coord) noexcept {
        Slot* slot = ensureSlot(coord);
        if (slot == nullptr) {
            return nullptr;
        }
        return &slot->chunk;
    }

    inline const SpatialChunk*
    SlidingWindowStorage::tryGetChunk(const ChunkCoord coord) const noexcept {
        const Slot* slot = tryGetSlot(coord);
        if (slot == nullptr) {
            return nullptr;
        }
        return &slot->chunk;
    }

    inline Cell* SlidingWindowStorage::cellsForChunk(SpatialChunk& chunk) noexcept {
        if (chunk.cellsBlockIndex == kInvalidCellsBlockIndex) {
            return nullptr;
        }
        return mCellsPool.data() + chunk.cellsBlockIndex;
    }

    inline const Cell*
    SlidingWindowStorage::cellsForChunk(const SpatialChunk& chunk) const noexcept {
        if (chunk.cellsBlockIndex == kInvalidCellsBlockIndex) {
            return nullptr;
        }
        return mCellsPool.data() + chunk.cellsBlockIndex;
    }

    template <typename Fn>
    inline bool SlidingWindowStorage::forEachChunkInRange(const ChunkCoord minCoord,
                                                          const ChunkCoord maxCoord,
                                                          Fn&& fn) const {
        const std::int32_t clampedMinX = std::max(minCoord.x, mOrigin.x);
        const std::int32_t clampedMinY = std::max(minCoord.y, mOrigin.y);
        const std::int32_t clampedMaxX = std::min(maxCoord.x, mMaxXInclusive);
        const std::int32_t clampedMaxY = std::min(maxCoord.y, mMaxYInclusive);

        if (clampedMinX > clampedMaxX || clampedMinY > clampedMaxY) {
            return true;
        }

        // Для строгих deterministic-query: отсутствие слота трактуем как Unloaded.
        SpatialChunk unloadedChunk{};
        unloadedChunk.cellsBlockIndex = kInvalidCellsBlockIndex;
        unloadedChunk.state = ResidencyState::Unloaded;
        detail::resetChunkOverlapCount(unloadedChunk);

        for (std::int32_t y = clampedMinY; y <= clampedMaxY; ++y) {
            for (std::int32_t x = clampedMinX; x <= clampedMaxX; ++x) {
                const ChunkCoord coord{x, y};

                const Slot* slot = tryGetSlot(coord);
                const SpatialChunk& chunkRef = (slot != nullptr) ? slot->chunk : unloadedChunk;

                if (!fn(coord, chunkRef)) {
                    return false;
                }
            }
        }

        return true;
    }

    template <typename Fn>
    inline bool SlidingWindowStorage::forEachChunkInRangeMut(const ChunkCoord minCoord,
                                                             const ChunkCoord maxCoord, Fn&& fn) {
        const std::int32_t clampedMinX = std::max(minCoord.x, mOrigin.x);
        const std::int32_t clampedMinY = std::max(minCoord.y, mOrigin.y);
        const std::int32_t clampedMaxX = std::min(maxCoord.x, mMaxXInclusive);
        const std::int32_t clampedMaxY = std::min(maxCoord.y, mMaxYInclusive);

        if (clampedMinX > clampedMaxX || clampedMinY > clampedMaxY) {
            return true;
        }

        for (std::int32_t y = clampedMinY; y <= clampedMaxY; ++y) {
            for (std::int32_t x = clampedMinX; x <= clampedMaxX; ++x) {
                const ChunkCoord coord{x, y};
                Slot* slot = ensureSlot(coord);
                if (slot == nullptr) {
                    continue;
                }
                if (!fn(coord, slot->chunk)) {
                    return false;
                }
            }
        }

        return true;
    }

    inline bool SlidingWindowStorage::setChunkState(const ChunkCoord coord,
                                                    const ResidencyState state) {
        if (!isInWindow(coord)) {
            return false;
        }

        Slot& slot = mSlots[slotIndex(coord)];

        if (slot.occupied && (slot.coord.x != coord.x || slot.coord.y != coord.y)) {
            // With fixed origin (Stage 3), this is impossible-state.
#if !defined(NDEBUG)
            assert(false && "SlidingWindowStorage: slot coord mismatch (internal error)");
#else
            LOG_PANIC(core::log::cat::ECS,
                      "SlidingWindowStorage: slot coord mismatch (internal error)");
#endif
        }

        if (!slot.occupied) {
            slot.coord = coord;
            slot.occupied = true;
            slot.chunk.state = ResidencyState::Unloaded;
            detail::resetChunkOverlapCount(slot.chunk);
            slot.chunk.cellsBlockIndex = kInvalidCellsBlockIndex;
        }

        const ResidencyState prev = slot.chunk.state;
        if (prev == state) {
            return true;
        }

        // Strict model:
        //  - Only Loaded chunks may hold cell data (and therefore must own an allocated block).
        if (prev == ResidencyState::Loaded && state != ResidencyState::Loaded) {
#if !defined(NDEBUG)
            assert(detail::chunkOverlapCountIsZero(slot.chunk) &&
                   "SlidingWindowStorage: unload with active entities (coverage != 0)");
#elif defined(SFML1_PROFILE)
            if (!detail::chunkOverlapCountIsZero(slot.chunk)) [[unlikely]] {
                LOG_PANIC(core::log::cat::ECS,
                          "SlidingWindowStorage: unload with active entities (coverage={})",
                          detail::chunkOverlapCountForLog(slot.chunk));
            }
#endif
            clearChunkDataInSlot(slot);

#if !defined(NDEBUG)
            assert(slot.chunk.cellsBlockIndex != kInvalidCellsBlockIndex &&
                   "SlidingWindowStorage: Loaded chunk must own a cells block");
#else
            if (slot.chunk.cellsBlockIndex == kInvalidCellsBlockIndex) [[unlikely]] {
                LOG_PANIC(core::log::cat::ECS,
                          "SlidingWindowStorage: Loaded chunk missing cells block");
            }
#endif
            releaseCellsBlock(slot.chunk.cellsBlockIndex);
            slot.chunk.cellsBlockIndex = kInvalidCellsBlockIndex;
        }

        if (prev != ResidencyState::Loaded && state == ResidencyState::Loaded) {
#if !defined(NDEBUG)
            assert(slot.chunk.cellsBlockIndex == kInvalidCellsBlockIndex &&
                   "SlidingWindowStorage: non-Loaded chunk must not own a cells block");
#else
            if (slot.chunk.cellsBlockIndex != kInvalidCellsBlockIndex) [[unlikely]] {
                LOG_PANIC(core::log::cat::ECS,
                          "SlidingWindowStorage: non-Loaded chunk owns a cells block");
            }
#endif

            const std::uint32_t newBlock = allocateCellsBlock();
#if !defined(NDEBUG)
            assert(newBlock != kInvalidCellsBlockIndex &&
                   "SlidingWindowStorage: cells pool exhausted (increase maxResidentChunks)");
#else
            if (newBlock == kInvalidCellsBlockIndex) [[unlikely]] {
                LOG_PANIC(
                    core::log::cat::ECS,
                    "SlidingWindowStorage: cells pool exhausted (increase maxResidentChunks)");
            }
#endif
            slot.chunk.cellsBlockIndex = newBlock;

            // Freshly loaded chunk starts empty.
            clearChunkDataInSlot(slot);
        }

        slot.chunk.state = state;

        if (state == ResidencyState::Unloaded) {
            // Treat Unloaded as absent slot (chunkState()==Unloaded anyway).
            slot.occupied = false;
        }

        return true;
    }

    inline ResidencyState SlidingWindowStorage::chunkState(const ChunkCoord coord) const noexcept {
        const Slot* slot = tryGetSlot(coord);
        if (slot == nullptr) {
            return ResidencyState::Unloaded;
        }
        return slot->chunk.state;
    }

    namespace detail {

        template <typename BoundsT> struct BoundsTraits;

        template <> struct BoundsTraits<Aabb2> final {
            using Scalar = float;

            [[nodiscard]] static bool isEmpty(const Aabb2& bounds) noexcept {
                return bounds.maxX < bounds.minX || bounds.maxY < bounds.minY;
            }

            // Inclusive bounds: min==max is a point.

            [[nodiscard]] static float maxInclusiveX(const Aabb2& bounds) noexcept {
                return bounds.maxX;
            }

            [[nodiscard]] static float maxInclusiveY(const Aabb2& bounds) noexcept {
                return bounds.maxY;
            }
        };

        template <> struct BoundsTraits<Aabb2i> final {
            using Scalar = std::int32_t;

            [[nodiscard]] static bool isEmpty(const Aabb2i& bounds) noexcept {
                return bounds.maxX <= bounds.minX || bounds.maxY <= bounds.minY;
            }

            [[nodiscard]] static std::int32_t maxInclusiveX(const Aabb2i& bounds) noexcept {
                return bounds.maxX - 1;
            }

            [[nodiscard]] static std::int32_t maxInclusiveY(const Aabb2i& bounds) noexcept {
                return bounds.maxY - 1;
            }
        };

        template <typename BoundsT>
        [[nodiscard]] inline std::int32_t floorDivScalar(const BoundsT value,
                                                         const std::int32_t divisor) noexcept {
            if constexpr (std::is_floating_point_v<BoundsT>) {
                return static_cast<std::int32_t>(
                    std::floor(static_cast<double>(value) / static_cast<double>(divisor)));
            } else {
                return detail::floorDivInt(static_cast<std::int32_t>(value), divisor);
            }
        }

    } // namespace detail

    template <typename Storage, typename BoundsT>
    inline bool
    SpatialIndexV2<Storage, BoundsT>::validateConfig(const SpatialIndexConfig& config) const {
        if (config.chunkSizeWorld <= 0 || config.cellSizeWorld <= 0) {
            return false;
        }
        if (config.chunkSizeWorld % config.cellSizeWorld != 0) {
            return false;
        }
        if (config.maxEntityId == 0) {
            return false;
        }
        if (config.overflow.nodeCapacity == 0 || config.overflow.maxNodes == 0) {
            return false;
        }
        return true;
    }

    template <typename Storage, typename BoundsT>
    inline void
    SpatialIndexV2<Storage, BoundsT>::init(const SpatialIndexConfig& config,
                                           const typename Storage::Config& storageConfig) {
        mConfig = config;
        mActiveEntityCount = 0;

#if !defined(NDEBUG)
        assert(validateConfig(config) && "SpatialIndexV2: invalid config");
#else
        if (!validateConfig(config)) {
            LOG_PANIC(core::log::cat::ECS, "SpatialIndexV2: invalid config");
        }
#endif

        mCellsPerChunkX = config.chunkSizeWorld / config.cellSizeWorld;
        mCellsPerChunkY = config.chunkSizeWorld / config.cellSizeWorld;
        mCellsPerChunk = static_cast<std::size_t>(mCellsPerChunkX) * mCellsPerChunkY;

        prepareStorage(storageConfig);
        mOverflowPool.init(config.overflow);
        prewarmEntityCapacity(config.maxEntityId);
        prewarmMarksCapacity(config.maxEntityId);
    }

    template <typename Storage, typename BoundsT>
    inline void SpatialIndexV2<Storage, BoundsT>::prepareStorage(
        const typename Storage::Config& storageConfig) {
        mStorage.bindOverflowPool(&mOverflowPool);
        mStorage.init(storageConfig, mCellsPerChunkX, mCellsPerChunkY);
    }

    template <typename Storage, typename BoundsT>
    inline void SpatialIndexV2<Storage, BoundsT>::prewarm(const std::uint32_t maxEntityId) {
        prewarmEntityCapacity(maxEntityId);
        prewarmMarksCapacity(maxEntityId);
    }

    template <typename Storage, typename BoundsT>
    inline void
    SpatialIndexV2<Storage, BoundsT>::prewarmEntityCapacity(const std::uint32_t maxEntityId) {
        if (maxEntityId == 0) {
            return;
        }
        mEntities.resize(static_cast<std::size_t>(maxEntityId) + 1u);
    }

    template <typename Storage, typename BoundsT>
    inline void
    SpatialIndexV2<Storage, BoundsT>::prewarmMarksCapacity(const std::uint32_t maxEntityId) {
        if (maxEntityId == 0) {
            return;
        }
        mMarks.resize(static_cast<std::size_t>(maxEntityId) + 1u, 0u);
    }

    template <typename Storage, typename BoundsT>
    inline void SpatialIndexV2<Storage, BoundsT>::ensureEntityStorage(const EntityId32 id) {
        if (id < mEntities.size()) {
            return;
        }
#if !defined(NDEBUG)
        assert(false && "SpatialIndexV2: entity storage not prewarmed (call prewarm)");
#else
        LOG_PANIC(core::log::cat::ECS, "SpatialIndexV2: entity storage not prewarmed (id={})", id);
#endif
    }

    template <typename Storage, typename BoundsT>
    inline void SpatialIndexV2<Storage, BoundsT>::ensureMarksStorage(const EntityId32 id) {
        if (id < mMarks.size()) {
            return;
        }
#if !defined(NDEBUG)
        assert(false && "SpatialIndexV2: marks storage not prewarmed (call prewarm)");
#else
        LOG_PANIC(core::log::cat::ECS, "SpatialIndexV2: marks storage not prewarmed (id={})", id);
#endif
    }

    template <typename Storage, typename BoundsT>
    inline typename SpatialIndexV2<Storage, BoundsT>::CellRange
    SpatialIndexV2<Storage, BoundsT>::computeCellRange(const BoundsT& bounds) const noexcept {
        using Traits = detail::BoundsTraits<BoundsT>;
        using Scalar = typename Traits::Scalar;

        CellRange range{};
        if (Traits::isEmpty(bounds)) {
            return range;
        }

        const Scalar maxX = Traits::maxInclusiveX(bounds);
        const Scalar maxY = Traits::maxInclusiveY(bounds);

        range.minX = detail::floorDivScalar(bounds.minX, mConfig.cellSizeWorld);
        range.minY = detail::floorDivScalar(bounds.minY, mConfig.cellSizeWorld);
        range.maxX = detail::floorDivScalar(maxX, mConfig.cellSizeWorld);
        range.maxY = detail::floorDivScalar(maxY, mConfig.cellSizeWorld);
        return range;
    }

    template <typename Storage, typename BoundsT>
    inline typename SpatialIndexV2<Storage, BoundsT>::CellRange
    SpatialIndexV2<Storage, BoundsT>::computeChunkRange(const BoundsT& bounds) const noexcept {
        using Traits = detail::BoundsTraits<BoundsT>;
        using Scalar = typename Traits::Scalar;

        CellRange range{};
        if (Traits::isEmpty(bounds)) {
            return range;
        }

        const Scalar maxX = Traits::maxInclusiveX(bounds);
        const Scalar maxY = Traits::maxInclusiveY(bounds);

        const WorldPosT<Scalar> minPos{bounds.minX, bounds.minY};
        const WorldPosT<Scalar> maxPos{maxX, maxY};

        const ChunkCoord minChunk = worldToChunk(minPos, mConfig.chunkSizeWorld);
        const ChunkCoord maxChunk = worldToChunk(maxPos, mConfig.chunkSizeWorld);

        range.minX = minChunk.x;
        range.minY = minChunk.y;
        range.maxX = maxChunk.x;
        range.maxY = maxChunk.y;
        return range;
    }

    template <typename Storage, typename BoundsT>
    inline bool SpatialIndexV2<Storage, BoundsT>::canWriteRange(const CellRange& chunkRange) const {
        for (std::int32_t y = chunkRange.minY; y <= chunkRange.maxY; ++y) {
            for (std::int32_t x = chunkRange.minX; x <= chunkRange.maxX; ++x) {
                if (mStorage.chunkState({x, y}) != ResidencyState::Loaded) {
                    return false;
                }
            }
        }
        return true;
    }

    template <typename Storage, typename BoundsT>
    inline WriteResult SpatialIndexV2<Storage, BoundsT>::registerEntity(const EntityId32 id,
                                                                        const BoundsT& bounds) {
        ensureEntityStorage(id);
        ensureMarksStorage(id);

        EntityRecord& record = mEntities[id];
#if !defined(NDEBUG)
        assert(!record.registered && "SpatialIndexV2::registerEntity: already registered");
#else
        if (record.registered) {
            LOG_PANIC(core::log::cat::ECS,
                      "SpatialIndexV2::registerEntity: entity already registered (id={})", id);
        }
#endif

        const CellRange chunkRange = computeChunkRange(bounds);
        if (chunkRange.empty()) {
            record.bounds = bounds;
            record.registered = true;
            assert(mActiveEntityCount < std::numeric_limits<std::uint32_t>::max() &&
                   "SpatialIndexV2: activeEntityCount overflow");
            ++mActiveEntityCount;
            return WriteResult::Success;
        }

        if (!canWriteRange(chunkRange)) {
            return WriteResult::Rejected;
        }

        const CellRange cellRange = computeCellRange(bounds);
        std::int32_t lastWrittenX = chunkRange.minX;
        std::int32_t lastWrittenY = chunkRange.minY - 1;

        for (std::int32_t y = chunkRange.minY; y <= chunkRange.maxY; ++y) {
            for (std::int32_t x = chunkRange.minX; x <= chunkRange.maxX; ++x) {
                SpatialChunk* chunk = mStorage.tryGetChunk({x, y});
#if !defined(NDEBUG)
                assert(chunk != nullptr && "SpatialIndexV2: chunk nullptr after canWriteRange");
#else
                if (chunk == nullptr) [[unlikely]] {
                    LOG_PANIC(core::log::cat::ECS,
                              "SpatialIndexV2: chunk nullptr after canWriteRange ({}, {})", x, y);
                }
#endif

                const WriteResult chunkResult =
                    writeToChunk(*chunk, {x, y}, cellRange, id);

                if (chunkResult == WriteResult::Rejected) {
                    // Rollback ONLY chunks we successfully wrote to (important: avoid removing
                    // from unrelated overflow chains in a chunk we never touched).
                    if (lastWrittenY >= chunkRange.minY) {
                        for (std::int32_t ry = chunkRange.minY; ry <= lastWrittenY; ++ry) {
                            const std::int32_t maxX =
                                (ry == lastWrittenY) ? lastWrittenX : chunkRange.maxX;
                            for (std::int32_t rx = chunkRange.minX; rx <= maxX; ++rx) {
                                SpatialChunk* rollbackChunk = mStorage.tryGetChunk({rx, ry});
#if !defined(NDEBUG)
                                assert(rollbackChunk != nullptr &&
                                       "SpatialIndexV2: rollback chunk nullptr (internal error)");
#else
                                if (rollbackChunk == nullptr) [[unlikely]] {
                                    LOG_PANIC(core::log::cat::ECS,
                                              "SpatialIndexV2: rollback chunk nullptr ({}, {})",
                                              rx, ry);
                                }
#endif
                                removeFromChunk(*rollbackChunk, {rx, ry}, cellRange, id);
                                detail::decrementChunkOverlapCount(*rollbackChunk);
                            }
                        }
                    }

                    return WriteResult::Rejected;
                }

                detail::incrementChunkOverlapCount(*chunk);
                lastWrittenX = x;
                lastWrittenY = y;
            }
        }

        record.bounds = bounds;
        record.registered = true;
        assert(mActiveEntityCount < std::numeric_limits<std::uint32_t>::max() &&
               "SpatialIndexV2: activeEntityCount overflow");
        ++mActiveEntityCount;
        return WriteResult::Success;
    }

    template <typename Storage, typename BoundsT>
    inline WriteResult SpatialIndexV2<Storage, BoundsT>::updateEntity(const EntityId32 id,
                                                                      const BoundsT& newBounds) {
        if (id >= mEntities.size()) {
            return WriteResult::Rejected;
        }

        EntityRecord& record = mEntities[id];
        if (!record.registered) {
            return WriteResult::Rejected;
        }

        const CellRange oldChunkRange = computeChunkRange(record.bounds);
        const CellRange newChunkRange = computeChunkRange(newBounds);

        // Strict model: old coverage must be accessible to be removed/adjusted.
        if (!oldChunkRange.empty() && !canWriteRange(oldChunkRange)) {
            return WriteResult::Rejected;
        }

        if (!newChunkRange.empty() && !canWriteRange(newChunkRange)) {
            return WriteResult::Rejected;
        }

        const CellRange oldCellRange = computeCellRange(record.bounds);
        const CellRange newCellRange = computeCellRange(newBounds);

        std::int32_t lastAddedX = newChunkRange.minX;
        std::int32_t lastAddedY = newChunkRange.minY - 1;

        // Phase ADD: chunks in new \ old
        for (std::int32_t y = newChunkRange.minY; y <= newChunkRange.maxY; ++y) {
            for (std::int32_t x = newChunkRange.minX; x <= newChunkRange.maxX; ++x) {
                const bool inOld = x >= oldChunkRange.minX && x <= oldChunkRange.maxX &&
                                   y >= oldChunkRange.minY && y <= oldChunkRange.maxY;
                if (inOld) {
                    continue;
                }

                SpatialChunk* chunk = mStorage.tryGetChunk({x, y});
#if !defined(NDEBUG)
                assert(chunk != nullptr &&
                       "SpatialIndexV2: chunk nullptr after canWriteRange (ADD)");
#else
                if (chunk == nullptr) [[unlikely]] {
                    LOG_PANIC(core::log::cat::ECS,
                              "SpatialIndexV2: chunk nullptr after canWriteRange (ADD) ({}, {})",
                              x, y);
                }
#endif

                const WriteResult addResult =
                    writeToChunk(*chunk, {x, y}, newCellRange, id);

                if (addResult == WriteResult::Rejected) {
                    // Rollback ONLY chunks we successfully added (avoid touching unrelated
                    // overflow chains in chunks we never modified).
                    if (lastAddedY >= newChunkRange.minY) {
                        for (std::int32_t ry = newChunkRange.minY; ry <= lastAddedY; ++ry) {
                            const std::int32_t maxX =
                                (ry == lastAddedY) ? lastAddedX : newChunkRange.maxX;
                            for (std::int32_t rx = newChunkRange.minX; rx <= maxX; ++rx) {
                                const bool inOldRollback =
                                    rx >= oldChunkRange.minX && rx <= oldChunkRange.maxX &&
                                    ry >= oldChunkRange.minY && ry <= oldChunkRange.maxY;
                                if (inOldRollback) {
                                    continue;
                                }

                                SpatialChunk* rollbackChunk = mStorage.tryGetChunk({rx, ry});
#if !defined(NDEBUG)
                                assert(rollbackChunk != nullptr &&
                                       "SpatialIndexV2: rollback chunk nullptr (ADD rollback)");
#else
                                if (rollbackChunk == nullptr) [[unlikely]] {
                                    LOG_PANIC(core::log::cat::ECS,
                                              "SpatialIndexV2: rollback chunk nullptr "
                                              "(ADD rollback) "
                                              "({}, {})",
                                              rx, ry);
                                }
#endif
                                removeFromChunk(*rollbackChunk, {rx, ry}, newCellRange, id);
                                detail::decrementChunkOverlapCount(*rollbackChunk);
                            }
                        }
                    }

                    return WriteResult::Rejected;
                }

                detail::incrementChunkOverlapCount(*chunk);
                lastAddedX = x;
                lastAddedY = y;
            }
        }

        // Phase KEEP: chunks in old ∩ new
        for (std::int32_t y = oldChunkRange.minY; y <= oldChunkRange.maxY; ++y) {
            for (std::int32_t x = oldChunkRange.minX; x <= oldChunkRange.maxX; ++x) {
                const bool inNew = x >= newChunkRange.minX && x <= newChunkRange.maxX &&
                                   y >= newChunkRange.minY && y <= newChunkRange.maxY;
                if (!inNew) {
                    continue;
                }

                SpatialChunk* chunk = mStorage.tryGetChunk({x, y});
#if !defined(NDEBUG)
                assert(chunk != nullptr &&
                       "SpatialIndexV2: chunk nullptr after canWriteRange (KEEP)");
#else
                if (chunk == nullptr) [[unlikely]] {
                    LOG_PANIC(core::log::cat::ECS,
                              "SpatialIndexV2: chunk nullptr after canWriteRange (KEEP) ({}, {})",
                              x, y);
                }
#endif

                const CellRange oldLocal = clampCellRangeToChunk(oldCellRange, {x, y});
                const CellRange newLocal = clampCellRangeToChunk(newCellRange, {x, y});
                if (newLocal.empty() && oldLocal.empty()) {
                    continue;
                }

                Cell* cells = cellsForChunk(*chunk);
#if !defined(NDEBUG)
                assert(cells != nullptr && "SpatialIndexV2: cellsForChunk nullptr (KEEP)");
#else
                if (cells == nullptr) [[unlikely]] {
                    LOG_PANIC(core::log::cat::ECS, "SpatialIndexV2: cellsForChunk nullptr (KEEP)");
                }
#endif

                // ADD first: newCells \ oldCells
                for (std::int32_t cy = newLocal.minY; cy <= newLocal.maxY; ++cy) {
                    const std::size_t rowOffset = static_cast<std::size_t>(cy) * mCellsPerChunkX;
                    for (std::int32_t cx = newLocal.minX; cx <= newLocal.maxX; ++cx) {
                        if (cx >= oldLocal.minX && cx <= oldLocal.maxX && cy >= oldLocal.minY &&
                            cy <= oldLocal.maxY) {
                            continue;
                        }

                        appendToCell(cells[rowOffset + cx], id);
                    }
                }

                // REMOVE second: oldCells \ newCells
                for (std::int32_t cy = oldLocal.minY; cy <= oldLocal.maxY; ++cy) {
                    const std::size_t rowOffset = static_cast<std::size_t>(cy) * mCellsPerChunkX;
                    for (std::int32_t cx = oldLocal.minX; cx <= oldLocal.maxX; ++cx) {
                        if (cx >= newLocal.minX && cx <= newLocal.maxX && cy >= newLocal.minY &&
                            cy <= newLocal.maxY) {
                            continue;
                        }
                        removeFromCell(cells[rowOffset + cx], id);
                    }
                }
            }
        }

        // Phase REMOVE: chunks in old \ new
        for (std::int32_t y = oldChunkRange.minY; y <= oldChunkRange.maxY; ++y) {
            for (std::int32_t x = oldChunkRange.minX; x <= oldChunkRange.maxX; ++x) {
                const bool inNew = x >= newChunkRange.minX && x <= newChunkRange.maxX &&
                                   y >= newChunkRange.minY && y <= newChunkRange.maxY;
                if (inNew) {
                    continue;
                }

                SpatialChunk* chunk = mStorage.tryGetChunk({x, y});
#if !defined(NDEBUG)
                assert(chunk != nullptr &&
                       "SpatialIndexV2: chunk nullptr after canWriteRange (REMOVE)");
#else
                if (chunk == nullptr) [[unlikely]] {
                    LOG_PANIC(core::log::cat::ECS,
                              "SpatialIndexV2: chunk nullptr after canWriteRange (REMOVE) ({}, {})",
                              x, y);
                }
#endif

                removeFromChunk(*chunk, {x, y}, oldCellRange, id);
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
                detail::decrementChunkOverlapCount(*chunk);
#endif
            }
        }

        record.bounds = newBounds;
        return WriteResult::Success;
    }

    template <typename Storage, typename BoundsT>
    inline bool SpatialIndexV2<Storage, BoundsT>::unregisterEntity(const EntityId32 id) {
        if (id >= mEntities.size()) {
            return false;
        }

        EntityRecord& record = mEntities[id];
        if (!record.registered) {
            return false;
        }

        const CellRange chunkRange = computeChunkRange(record.bounds);
        if (!chunkRange.empty() && !canWriteRange(chunkRange)) {
            return false;
        }

        if (!chunkRange.empty()) {
            const CellRange cellRange = computeCellRange(record.bounds);

            for (std::int32_t y = chunkRange.minY; y <= chunkRange.maxY; ++y) {
                for (std::int32_t x = chunkRange.minX; x <= chunkRange.maxX; ++x) {
                    SpatialChunk* chunk = mStorage.tryGetChunk({x, y});
#if !defined(NDEBUG)
                    assert(chunk != nullptr &&
                           "SpatialIndexV2: chunk nullptr after canWriteRange (unregister)");
#else
                    if (chunk == nullptr) [[unlikely]] {
                        LOG_PANIC(core::log::cat::ECS,
                                  "SpatialIndexV2: chunk nullptr after canWriteRange (unregister) "
                                  "({}, {})",
                                  x, y);
                    }
#endif

                    removeFromChunk(*chunk, {x, y}, cellRange, id);
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
                    detail::decrementChunkOverlapCount(*chunk);
#endif
                }
            }
        }

        record.registered = false;
        assert(mActiveEntityCount > 0 && "SpatialIndexV2: activeEntityCount underflow");
        --mActiveEntityCount;
        return true;
    }

    template <typename Storage, typename BoundsT>
    inline WriteResult
    SpatialIndexV2<Storage, BoundsT>::writeToChunk(SpatialChunk& chunk,
                                                   const ChunkCoord coord,
                                                   const CellRange& cellRange,
                                                   const EntityId32 id) {
        if (chunk.state != ResidencyState::Loaded) {
            (void) coord;
            return WriteResult::Rejected;
        }

        forEachCellInRange(chunk, coord, cellRange, [&](Cell& cell) {
            appendToCell(cell, id); // void: either succeeds or PANICs
            return true;
        });

        return WriteResult::Success;
    }

    template <typename Storage, typename BoundsT>
    inline void SpatialIndexV2<Storage, BoundsT>::removeFromChunk(SpatialChunk& chunk,
                                                                  const ChunkCoord coord,
                                                                  const CellRange& cellRange,
                                                                  const EntityId32 id) noexcept {
        if (chunk.state != ResidencyState::Loaded) {
            (void) coord;
            return;
        }

        forEachCellInRange(chunk, coord, cellRange, [&](Cell& cell) {
            removeFromCell(cell, id);
            return true;
        });
    }

template <typename Storage, typename BoundsT>
    inline void SpatialIndexV2<Storage, BoundsT>::removeFromCell(Cell& cell,
                                                                 const EntityId32 id) noexcept {
        // Inline search.
        for (std::uint8_t i = 0; i < cell.count; ++i) {
            if (cell.entities[i] != id) {
                continue;
            }

            for (std::uint8_t j = static_cast<std::uint8_t>(i + 1u); j < cell.count; ++j) {
                // static_cast: расширяем j до size_t ДО вычитания —
                // silences lnt-arithmetic-overflow.
                // Loop invariant: j >= 1 (начинается с i + 1, где i >= 0).
                cell.entities[static_cast<std::size_t>(j) - 1u] = cell.entities[j];
            }
            --cell.count;
            return;
        }

        // Overflow search.
        if (cell.overflowHandle != 0u) {
            const bool removed = mOverflowPool.remove(cell.overflowHandle, id);
#if !defined(NDEBUG)
            assert(removed && "SpatialIndexV2: overflow remove failed");
#else
            if (!removed) [[unlikely]] {
                LOG_PANIC(core::log::cat::ECS, "SpatialIndexV2: overflow remove failed (id={})",
                          id);
            }
#endif
            return;
        }

        // Entity not found in inline nor overflow.
        // Write-path invariant: every cell that was written to MUST contain the entity.
        // Reaching here means data corruption (double-remove, cell stomp, etc.).
#if !defined(NDEBUG)
        assert(false &&
               "SpatialIndexV2: removeFromCell entity not found "
               "(write-path invariant violated)");
#else
        LOG_PANIC(core::log::cat::ECS,
                  "SpatialIndexV2: removeFromCell entity not found (id={})", id);
#endif
    }

    template <typename Storage, typename BoundsT>
    inline void SpatialIndexV2<Storage, BoundsT>::appendToCell(Cell& cell, const EntityId32 id) {
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        bool alreadyPresent = false;
        for (std::uint8_t i = 0; i < cell.count; ++i) {
            if (cell.entities[i] == id) {
                alreadyPresent = true;
                break;
            }
        }
        if (!alreadyPresent && cell.overflowHandle != 0u) {
            (void) mOverflowPool.forEach(cell.overflowHandle, [&](const EntityId32 existing) {
                if (existing == id) {
                    alreadyPresent = true;
                    return false;
                }
                return true;
            });
        }
        if (alreadyPresent) {
            LOG_PANIC(core::log::cat::ECS,
                      "SpatialIndexV2: duplicate entity in cell (id={}). "
                      "This indicates write-path corruption (failed remove or double-insert).",
                      id);
        }
#endif
        if (cell.count < Cell::kInlineCapacity) {
            cell.entities[cell.count] = id;
            ++cell.count;
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
            if (cell.count > mDebugWorstCellDensity) {
                mDebugWorstCellDensity = cell.count;
            }
#endif
            return;
        }

        const bool ok = mOverflowPool.append(cell.overflowHandle, id);
        if (!ok) {
            handleOverflowExhausted(); // [[noreturn]]
        }

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        const std::uint32_t overflowCount = mOverflowPool.totalCount(cell.overflowHandle);
        const std::uint32_t total = static_cast<std::uint32_t>(cell.count) + overflowCount;
        if (total > mDebugWorstCellDensity) {
            mDebugWorstCellDensity = total;
        }
#endif
    }

    template <typename Storage, typename BoundsT>
    inline bool SpatialIndexV2<Storage, BoundsT>::beginQueryStamp() const {
        if (mMarksClearRequired) {
            return false;
        }

        if (mQueryStamp == std::numeric_limits<std::uint32_t>::max()) {
            mMarksClearRequired = true;
            return false;
        }

        ++mQueryStamp;
        return true;
    }

    template <typename Storage, typename BoundsT>
    inline void SpatialIndexV2<Storage, BoundsT>::handleMarksWrap() const {
#if !defined(NDEBUG)
        LOG_CRITICAL(core::log::cat::ECS,
                     "SpatialIndexV2: marks maintenance required (call clearMarksTable)");
        assert(false && "SpatialIndexV2: marks maintenance required (call clearMarksTable)");
#else
        LOG_PANIC(core::log::cat::ECS,
                  "SpatialIndexV2: marks maintenance required (call clearMarksTable)");
#endif
    }

    template <typename Storage, typename BoundsT>
    inline void
    SpatialIndexV2<Storage, BoundsT>::handleNonLoadedChunk(const ChunkCoord coord) const {
#if !defined(NDEBUG)
        LOG_CRITICAL(core::log::cat::ECS,
                     "SpatialIndexV2: deterministic query on non-loaded chunk ({}, {})", coord.x,
                     coord.y);
        assert(false && "SpatialIndexV2: non-loaded chunk in deterministic query");
#else
        LOG_PANIC(core::log::cat::ECS,
                  "SpatialIndexV2: deterministic query on non-loaded chunk ({}, {})", coord.x,
                  coord.y);
#endif
    }

    template <typename Storage, typename BoundsT>
    [[noreturn]] inline void SpatialIndexV2<Storage, BoundsT>::handleOverflowExhausted() const {
        LOG_PANIC(core::log::cat::ECS,
                  "SpatialIndexV2: overflow pool exhausted (maxNodes={}). "
                  "Increase overflow pool capacity or reduce entity density/AABB coverage.",
                  mOverflowPool.maxNodes());
    }

    template <typename Storage, typename BoundsT>
    inline void SpatialIndexV2<Storage, BoundsT>::handleOutputOverflow() const {
#if !defined(NDEBUG)
        LOG_CRITICAL(core::log::cat::ECS, "SpatialIndexV2: query output buffer is too small");
        assert(false && "SpatialIndexV2: query output buffer is too small");
#else
        LOG_PANIC(core::log::cat::ECS, "SpatialIndexV2: query output buffer is too small");
#endif
    }

    template <typename Storage, typename BoundsT>
    inline void SpatialIndexV2<Storage, BoundsT>::clearMarksTable() const noexcept {
        std::fill(mMarks.begin(), mMarks.end(), 0u);
        mQueryStamp = 1;
        mMarksClearRequired = false;
    }

    template <typename Storage, typename BoundsT>
    template <typename ChunkT, typename Fn>
    inline bool
    SpatialIndexV2<Storage, BoundsT>::forEachCellInRange(ChunkT& chunk, const ChunkCoord coord,
                                                         const CellRange& cellRange, Fn&& fn) {
        const std::int64_t chunkCellOriginX = static_cast<std::int64_t>(coord.x) * mCellsPerChunkX;
        const std::int64_t chunkCellOriginY = static_cast<std::int64_t>(coord.y) * mCellsPerChunkY;

        const std::int64_t minX64 = static_cast<std::int64_t>(cellRange.minX) - chunkCellOriginX;
        const std::int64_t minY64 = static_cast<std::int64_t>(cellRange.minY) - chunkCellOriginY;
        const std::int64_t maxX64 = static_cast<std::int64_t>(cellRange.maxX) - chunkCellOriginX;
        const std::int64_t maxY64 = static_cast<std::int64_t>(cellRange.maxY) - chunkCellOriginY;

        const std::int64_t maxXClamp = static_cast<std::int64_t>(mCellsPerChunkX) - 1;
        const std::int64_t maxYClamp = static_cast<std::int64_t>(mCellsPerChunkY) - 1;
        constexpr std::int64_t kZero64 = std::int64_t{0};

        const std::int32_t localMinX =
            static_cast<std::int32_t>(std::clamp(minX64, kZero64, maxXClamp));
        const std::int32_t localMinY =
            static_cast<std::int32_t>(std::clamp(minY64, kZero64, maxYClamp));
        const std::int32_t localMaxX =
            static_cast<std::int32_t>(std::clamp(maxX64, kZero64, maxXClamp));
        const std::int32_t localMaxY =
            static_cast<std::int32_t>(std::clamp(maxY64, kZero64, maxYClamp));

        if (localMinX > localMaxX || localMinY > localMaxY) {
            return true;
        }

        auto* cells = cellsForChunk(chunk);
#if !defined(NDEBUG)
        assert(cells != nullptr && "SpatialIndexV2: cellsForChunk nullptr (internal error)");
#else
        if (cells == nullptr) [[unlikely]] {
            LOG_PANIC(core::log::cat::ECS,
                      "SpatialIndexV2: cellsForChunk nullptr (internal error)");
        }
#endif

        for (std::int32_t y = localMinY; y <= localMaxY; ++y) {
            const std::size_t rowOffset = static_cast<std::size_t>(y) * mCellsPerChunkX;
            for (std::int32_t x = localMinX; x <= localMaxX; ++x) {
                auto& cell = cells[rowOffset + static_cast<std::size_t>(x)];
                if (!fn(cell)) {
                    return false;
                }
            }
        }

        return true;
    }

    template <typename Storage, typename BoundsT>
    template <typename ChunkT, typename Fn>
    inline bool SpatialIndexV2<Storage, BoundsT>::forEachCellInRange(const ChunkT& chunk,
                                                                     const ChunkCoord coord,
                                                                     const CellRange& cellRange,
                                                                     Fn&& fn) const {
        const std::int64_t chunkCellOriginX = static_cast<std::int64_t>(coord.x) * mCellsPerChunkX;
        const std::int64_t chunkCellOriginY = static_cast<std::int64_t>(coord.y) * mCellsPerChunkY;

        const std::int64_t minX64 = static_cast<std::int64_t>(cellRange.minX) - chunkCellOriginX;
        const std::int64_t minY64 = static_cast<std::int64_t>(cellRange.minY) - chunkCellOriginY;
        const std::int64_t maxX64 = static_cast<std::int64_t>(cellRange.maxX) - chunkCellOriginX;
        const std::int64_t maxY64 = static_cast<std::int64_t>(cellRange.maxY) - chunkCellOriginY;

        const std::int64_t maxXClamp = static_cast<std::int64_t>(mCellsPerChunkX) - 1;
        const std::int64_t maxYClamp = static_cast<std::int64_t>(mCellsPerChunkY) - 1;
        constexpr std::int64_t kZero64 = std::int64_t{0};

        const std::int32_t localMinX =
            static_cast<std::int32_t>(std::clamp(minX64, kZero64, maxXClamp));
        const std::int32_t localMinY =
            static_cast<std::int32_t>(std::clamp(minY64, kZero64, maxYClamp));
        const std::int32_t localMaxX =
            static_cast<std::int32_t>(std::clamp(maxX64, kZero64, maxXClamp));
        const std::int32_t localMaxY =
            static_cast<std::int32_t>(std::clamp(maxY64, kZero64, maxYClamp));

        if (localMinX > localMaxX || localMinY > localMaxY) {
            return true;
        }

        auto* cells = cellsForChunk(chunk);
#if !defined(NDEBUG)
        assert(cells != nullptr && "SpatialIndexV2: cellsForChunk nullptr (internal error)");
#else
        if (cells == nullptr) [[unlikely]] {
            LOG_PANIC(core::log::cat::ECS,
                      "SpatialIndexV2: cellsForChunk nullptr (internal error)");
        }
#endif

        for (std::int32_t y = localMinY; y <= localMaxY; ++y) {
            const std::size_t rowOffset = static_cast<std::size_t>(y) * mCellsPerChunkX;
            for (std::int32_t x = localMinX; x <= localMaxX; ++x) {
                auto& cell = cells[rowOffset + static_cast<std::size_t>(x)];
                if (!fn(cell)) {
                    return false;
                }
            }
        }

        return true;
    }

    template <typename Storage, typename BoundsT>
    template <typename Fn>
    inline bool SpatialIndexV2<Storage, BoundsT>::forEachEntityInCell(const Cell& cell,
                                                                      Fn&& fn) const {
        for (std::uint8_t i = 0; i < cell.count; ++i) {
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
            ++mDebugLastQueryStats.entriesScanned;
#endif
            if (!fn(cell.entities[i])) {
                return false;
            }
        }

        if (cell.overflowHandle != 0u) {
            return mOverflowPool.forEach(cell.overflowHandle, [&](const EntityId32 id) {
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
                ++mDebugLastQueryStats.entriesScanned;
                ++mDebugLastQueryStats.overflowEntitiesVisited;
#endif
                return fn(id);
            });
        }

        return true;
    }

    template <typename Storage, typename BoundsT>
    inline typename SpatialIndexV2<Storage, BoundsT>::CellRange
    SpatialIndexV2<Storage, BoundsT>::clampCellRangeToChunk(const CellRange& range,
                                                            const ChunkCoord coord) const noexcept {
        CellRange local{};
        if (range.empty()) {
            return local;
        }

        const std::int64_t chunkCellOriginX = static_cast<std::int64_t>(coord.x) * mCellsPerChunkX;
        const std::int64_t chunkCellOriginY = static_cast<std::int64_t>(coord.y) * mCellsPerChunkY;

        const std::int64_t minX64 = static_cast<std::int64_t>(range.minX) - chunkCellOriginX;
        const std::int64_t minY64 = static_cast<std::int64_t>(range.minY) - chunkCellOriginY;
        const std::int64_t maxX64 = static_cast<std::int64_t>(range.maxX) - chunkCellOriginX;
        const std::int64_t maxY64 = static_cast<std::int64_t>(range.maxY) - chunkCellOriginY;

        const std::int64_t maxXClamp = static_cast<std::int64_t>(mCellsPerChunkX) - 1;
        const std::int64_t maxYClamp = static_cast<std::int64_t>(mCellsPerChunkY) - 1;
        constexpr std::int64_t kZero64 = std::int64_t{0};

        local.minX = static_cast<std::int32_t>(std::clamp(minX64, kZero64, maxXClamp));
        local.minY = static_cast<std::int32_t>(std::clamp(minY64, kZero64, maxYClamp));
        local.maxX = static_cast<std::int32_t>(std::clamp(maxX64, kZero64, maxXClamp));
        local.maxY = static_cast<std::int32_t>(std::clamp(maxY64, kZero64, maxYClamp));

        if (local.minX > local.maxX || local.minY > local.maxY) {
            return CellRange{};
        }

        return local;
    }

    template <typename Storage, typename BoundsT>
    inline Cell* SpatialIndexV2<Storage, BoundsT>::cellsForChunk(SpatialChunk& chunk) noexcept {
        return mStorage.cellsForChunk(chunk);
    }

    template <typename Storage, typename BoundsT>
    inline const Cell*
    SpatialIndexV2<Storage, BoundsT>::cellsForChunk(const SpatialChunk& chunk) const noexcept {
        return mStorage.cellsForChunk(chunk);
    }

    template <typename Storage, typename BoundsT>
    inline std::size_t
    SpatialIndexV2<Storage, BoundsT>::queryFast(const BoundsT& area,
                                                std::span<EntityId32> out) const {
        if (!beginQueryStamp()) {
            return 0;
        }
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        mDebugLastQueryStats = DebugQueryStats{};
#endif
        const CellRange chunkRange = computeChunkRange(area);
        if (chunkRange.empty()) {
            return 0;
        }
        const CellRange cellRange = computeCellRange(area);
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        mDebugLastQueryStats.chunkMinX = chunkRange.minX;
        mDebugLastQueryStats.chunkMinY = chunkRange.minY;
        mDebugLastQueryStats.chunkMaxX = chunkRange.maxX;
        mDebugLastQueryStats.chunkMaxY = chunkRange.maxY;
        mDebugLastQueryStats.cellMinX = cellRange.minX;
        mDebugLastQueryStats.cellMinY = cellRange.minY;
        mDebugLastQueryStats.cellMaxX = cellRange.maxX;
        mDebugLastQueryStats.cellMaxY = cellRange.maxY;
#endif

        std::size_t outCount = 0;
        bool stop = false;

        mStorage.forEachChunkInRange(
            {chunkRange.minX, chunkRange.minY}, {chunkRange.maxX, chunkRange.maxY},
            [&](const ChunkCoord coord, const SpatialChunk& chunk) {
                if (stop) {
                    return false;
                }
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
                ++mDebugLastQueryStats.chunksVisited;
#endif
                if (chunk.state != ResidencyState::Loaded) {
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
                    ++mDebugLastQueryStats.chunksSkippedNonLoaded;
#endif
                    return true;
                }
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
                ++mDebugLastQueryStats.chunksLoadedVisited;
#endif
                return forEachCellInRange(chunk, coord, cellRange, [&](const Cell& cell) {
                    if (stop) {
                        return false;
                    }
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
                    ++mDebugLastQueryStats.cellVisits;
                    if (cell.overflowHandle != 0u) {
                        ++mDebugLastQueryStats.overflowCellVisits;
                    }
#endif
                    return forEachEntityInCell(cell, [&](const EntityId32 id) {
                        if (outCount >= out.size()) {
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
                            mDebugLastQueryStats.outTruncated = 1;
#endif
                            stop = true;
                            return false;
                        }
#if !defined(NDEBUG)
                        assert(id < mMarks.size() && "SpatialIndexV2: marks table too small");
#endif
                        if (id >= mMarks.size()) [[unlikely]] {
                            LOG_PANIC(core::log::cat::ECS,
                                      "SpatialIndexV2: marks table too small (id={})", id);
                        }
                        if (mMarks[id] == mQueryStamp) {
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
                            ++mDebugLastQueryStats.dupHits;
#endif
                            return true;
                        }
                        mMarks[id] = mQueryStamp;
                        out[outCount++] = id;
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
                        ++mDebugLastQueryStats.uniqueAdded;
#endif
                        return true;
                    });
                });
            });

        return outCount;
    }

    template <typename Storage, typename BoundsT>
    inline void SpatialIndexV2<Storage, BoundsT>::collectEntityIdsInChunk(
        const ChunkCoord coord, std::vector<EntityId32>& outIdsScratch) const {
        outIdsScratch.clear();

        if (!beginQueryStamp()) {
#if defined(NDEBUG)
            handleMarksWrap();
#else
            handleMarksWrap();
            return;
#endif
        }

        const SpatialChunk* chunk = mStorage.tryGetChunk(coord);
        if (chunk == nullptr || chunk->state != ResidencyState::Loaded) {
            return;
        }

        const Cell* cells = cellsForChunk(*chunk);
        if (cells == nullptr) {
            return;
        }

        for (std::size_t i = 0; i < mCellsPerChunk; ++i) {
            const Cell& cell = cells[i];
            (void) forEachEntityInCell(cell, [&](const EntityId32 id) {
#if !defined(NDEBUG)
                assert(id < mMarks.size() && "SpatialIndexV2: marks table too small");
#endif
                if (mMarks[id] == mQueryStamp) {
                    return true;
                }
                mMarks[id] = mQueryStamp;
                outIdsScratch.push_back(id);
                return true;
            });
        }
    }

    template <typename Storage, typename BoundsT>
    inline std::size_t
    SpatialIndexV2<Storage, BoundsT>::queryDeterministic(const BoundsT& area,
                                                         std::span<EntityId32> out) const {
        if (!beginQueryStamp()) {
#if defined(NDEBUG)
            handleMarksWrap();
#else
            handleMarksWrap();
            return 0;
#endif
        }
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        mDebugLastQueryStats = DebugQueryStats{};
#endif
        const CellRange chunkRange = computeChunkRange(area);
        if (chunkRange.empty()) {
            return 0;
        }
        const CellRange cellRange = computeCellRange(area);

        std::size_t outCount = 0;
        bool stop = false;

        mStorage.forEachChunkInRange(
            {chunkRange.minX, chunkRange.minY}, {chunkRange.maxX, chunkRange.maxY},
            [&](const ChunkCoord coord, const SpatialChunk& chunk) {
                if (stop) {
                    return false;
                }
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
                ++mDebugLastQueryStats.chunksVisited;
#endif
                if (chunk.state != ResidencyState::Loaded) {
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
                    ++mDebugLastQueryStats.chunksSkippedNonLoaded;
#endif
#if defined(NDEBUG)
                    handleNonLoadedChunk(coord);
#else
                    handleNonLoadedChunk(coord);
                    stop = true;
                    return false;
#endif
                }
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
                ++mDebugLastQueryStats.chunksLoadedVisited;
#endif
                return forEachCellInRange(chunk, coord, cellRange, [&](const Cell& cell) {
                    if (stop) {
                        return false;
                    }
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
                    ++mDebugLastQueryStats.cellVisits;
                    if (cell.overflowHandle != 0u) {
                        ++mDebugLastQueryStats.overflowCellVisits;
                    }
#endif
                    return forEachEntityInCell(cell, [&](const EntityId32 id) {
                        if (outCount >= out.size()) {
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
                            mDebugLastQueryStats.outTruncated = 1;
#endif
                            handleOutputOverflow(); // [[noreturn]]
#if !defined(NDEBUG)
                            // Defensive (unreachable after PANIC)
                            stop = true;
                            return false;
#endif
                        }
#if !defined(NDEBUG)
                        assert(id < mMarks.size() && "SpatialIndexV2: marks table too small");
#endif
                        if (id >= mMarks.size()) [[unlikely]] {
                            LOG_PANIC(core::log::cat::ECS,
                                      "SpatialIndexV2: marks table too small (id={})", id);
                        }
                        if (mMarks[id] == mQueryStamp) {
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
                            ++mDebugLastQueryStats.dupHits;
#endif
                            return true;
                        }
                        mMarks[id] = mQueryStamp;
                        out[outCount++] = id;
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
                        ++mDebugLastQueryStats.uniqueAdded;
#endif
                        return true;
                    });
                });
            });

        return outCount;
    }

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
    template <typename Storage, typename BoundsT>
    inline typename SpatialIndexV2<Storage, BoundsT>::DebugCellHealth
    SpatialIndexV2<Storage, BoundsT>::debugCellHealthForChunk(
        const ChunkCoord coord) const noexcept {
        DebugCellHealth out{};
        out.totalCells = static_cast<std::uint32_t>(mCellsPerChunk);

        const SpatialChunk* chunk = mStorage.tryGetChunk(coord);
        if (chunk == nullptr || chunk->state != ResidencyState::Loaded) {
            return out;
        }
        out.loaded = 1;

        const Cell* cells = cellsForChunk(*chunk);
        if (cells == nullptr) {
            return out;
        }

        constexpr std::uint32_t kSampleCap = 64;
        std::array<EntityId32, kSampleCap> sample{};

        for (std::size_t i = 0; i < mCellsPerChunk; ++i) {
            const Cell& cell = cells[i];

            std::uint32_t len = static_cast<std::uint32_t>(cell.count);

            // Если inline уже > cap — мы даже не пытаемся искать dupApprox.
            bool sampleEligible = (len <= kSampleCap);
            std::uint32_t sampleCount = 0;

            if (sampleEligible) {
                for (std::uint8_t j = 0; j < cell.count; ++j) {
                    sample[sampleCount++] = cell.entities[j];
                }
            }

            if (cell.overflowHandle != 0u) {
                std::uint32_t overflowCount = 0;

                (void) mOverflowPool.forEach(cell.overflowHandle, [&](const EntityId32 id) {
                    ++overflowCount;

                    // Сэмплим, пока итоговая длина не гарантированно > cap.
                    if (sampleEligible) {
                        if (sampleCount < kSampleCap) {
                            sample[sampleCount++] = id;
                        } else {
                            // Вылезли за cap => dupApprox не считаем.
                            sampleEligible = false;
                        }
                    }
                    return true;
                });

                len += overflowCount;
                if (len > kSampleCap) {
                    sampleEligible = false;
                }
            }

            out.sumCellLen += len;
            if (len > out.maxCellLen) {
                out.maxCellLen = len;
            }

            if (len == 0) {
                continue;
            }

            if (sampleEligible && sampleCount > 1u) {
                std::sort(sample.begin(), sample.begin() + static_cast<std::ptrdiff_t>(sampleCount));
                for (std::uint32_t k = 1; k < sampleCount; ++k) {
                    if (sample[k - 1u] == sample[k]) {
                        ++out.dupApprox;
                    }
                }
            }
        }

        return out;
    }

    template <typename Storage, typename BoundsT>
    inline typename SpatialIndexV2<Storage, BoundsT>::DebugStats
    SpatialIndexV2<Storage, BoundsT>::debugStats() const noexcept {
        return DebugStats{.lastQuery = mDebugLastQueryStats,
                          .worstCellDensity = mDebugWorstCellDensity,
                          .dupInsertDetected = mDebugDupInsertDetected,
                          .overflow = mOverflowPool.debugStats()};
    }
#endif

} // namespace core::spatial