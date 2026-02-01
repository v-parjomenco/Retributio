diff --git a/SFML1/SFML1.vcxproj b/SFML1/SFML1.vcxproj
index 12c7639..7c05388 100644
--- a/SFML1/SFML1.vcxproj
+++ b/SFML1/SFML1.vcxproj
@@ -477,6 +477,7 @@
     <ClCompile Include="src\game\skyguard\config\loader\user_settings_loader.cpp" />
     <ClCompile Include="src\game\skyguard\config\loader\view_config_loader.cpp" />
     <ClCompile Include="src\game\skyguard\config\loader\window_config_loader.cpp" />
+    <ClCompile Include="src\game\skyguard\dev\spatial_index_harness.cpp" />
     <ClCompile Include="src\game\skyguard\dev\stress_scene.cpp" />
     <ClCompile Include="src\game\skyguard\ecs\systems\player_init_system.cpp" />
     <ClCompile Include="src\game\skyguard\game.cpp" />
@@ -533,6 +534,7 @@
     <ClInclude Include="include\core\ecs\components\scaling_behavior_component.h" />
     <ClInclude Include="include\core\ecs\components\spatial_dirty_tag.h" />
     <ClInclude Include="include\core\ecs\components\spatial_handle_component.h" />
+    <ClInclude Include="include\core\ecs\components\spatial_id_component.h" />
     <ClInclude Include="include\core\ecs\components\sprite_component.h" />
     <ClInclude Include="include\core\ecs\components\sprite_scaling_data_component.h" />
     <ClInclude Include="include\core\ecs\components\transform_component.h" />
@@ -601,6 +603,7 @@
     <ClInclude Include="include\game\skyguard\config\properties\aircraft_control_properties.h" />
     <ClInclude Include="include\game\skyguard\config\view_config.h" />
     <ClInclude Include="include\game\skyguard\config\window_config.h" />
+    <ClInclude Include="include\game\skyguard\dev\spatial_index_harness.h" />
     <ClInclude Include="include\game\skyguard\dev\stress_scene.h" />
     <ClInclude Include="include\game\skyguard\ecs\components\aircraft_control_bindings_component.h" />
     <ClInclude Include="include\game\skyguard\ecs\components\aircraft_control_component.h" />
diff --git a/SFML1/SFML1.vcxproj.filters b/SFML1/SFML1.vcxproj.filters
index 514f649..b7ed177 100644
--- a/SFML1/SFML1.vcxproj.filters
+++ b/SFML1/SFML1.vcxproj.filters
@@ -444,6 +444,9 @@
     <ClCompile Include="src\core\spatial\spatial_index_v2.cpp">
       <Filter>src\core\spatial</Filter>
     </ClCompile>
+    <ClCompile Include="src\game\skyguard\dev\spatial_index_harness.cpp">
+      <Filter>src\game\skyguard\dev</Filter>
+    </ClCompile>
   </ItemGroup>
   <ItemGroup>
     <ClInclude Include="include\core\resources\resource_manager.h">
@@ -776,6 +779,12 @@
     <ClInclude Include="include\core\spatial\spatial_index_v2.h">
       <Filter>include\core\spatial</Filter>
     </ClInclude>
+    <ClInclude Include="include\core\ecs\components\spatial_id_component.h">
+      <Filter>include\core\ecs\components</Filter>
+    </ClInclude>
+    <ClInclude Include="include\game\skyguard\dev\spatial_index_harness.h">
+      <Filter>include\game\skyguard\dev</Filter>
+    </ClInclude>
   </ItemGroup>
   <ItemGroup>
     <None Include="assets\core\config\debug_overlay.json">
diff --git a/SFML1/include/core/ecs/stable_id_service.h b/SFML1/include/core/ecs/stable_id_service.h
index ecc9332..622851c 100644
--- a/SFML1/include/core/ecs/stable_id_service.h
+++ b/SFML1/include/core/ecs/stable_id_service.h
@@ -35,6 +35,7 @@ namespace core::ecs {
 
         void onEntityDestroyed(Entity e) noexcept;
         void clear() noexcept;
+        void prewarm(std::size_t maxEntities);
 
       private:
         static constexpr StableId kUnsetStableId = 0u;
diff --git a/SFML1/include/core/ecs/systems/render_system.h b/SFML1/include/core/ecs/systems/render_system.h
index b387dec..51d96bd 100644
--- a/SFML1/include/core/ecs/systems/render_system.h
+++ b/SFML1/include/core/ecs/systems/render_system.h
@@ -2,13 +2,14 @@
 // File: core/ecs/systems/render_system.h
 // Purpose: High-performance batched sprite rendering with spatial culling
 // Used by: Game layer (render loop)
-// Related headers: world.h, spatial_index.h, resource_manager.h
+// Related headers: world.h, spatial_index_v2.h, resource_manager.h
 // ================================================================================================
 #pragma once
 
 #include <cstddef>
 #include <cstdint>
 #include <memory>
+#include <span>
 #include <vector>
 
 #include <SFML/Graphics/Rect.hpp>
@@ -18,6 +19,7 @@
 #include "core/ecs/entity.h"
 #include "core/ecs/system.h"
 #include "core/resources/keys/resource_key.h"
+#include "core/spatial/spatial_index_v2.h"
 
 namespace sf {
     class RenderWindow;
@@ -28,10 +30,6 @@ namespace core::resources {
     class ResourceManager;
 } // namespace core::resources
 
-namespace core::spatial {
-    class SpatialIndex;
-} // namespace core::spatial
-
 namespace core::ecs {
 
     class World;
@@ -103,9 +101,10 @@ namespace core::ecs {
          * Дополнительно:
          *  - bind(nullptr, nullptr) допустим как явный "unbind" (сбрасывает кэш и состояние).
          *  - bind(spatialIndex, nullptr) / bind(nullptr, resources) — misuse -> (LOG_PANIC).
+         *  - entitiesBySpatialId must be non-empty when spatialIndex is provided.
          */
-        void bind(const core::spatial::SpatialIndex* spatialIndex,
-                  const core::resources::ResourceManager* resources);
+        void bind(const core::spatial::SpatialIndexV2Flat* spatialIndex,
+                  std::span<const Entity> entitiesBySpatialId,
 
         /**
          * @brief Логическое обновление (не используется, рендер-only система).
@@ -142,7 +141,11 @@ namespace core::ecs {
         // ----------------------------------------------------------------------------------------
 
         /// Spatial index для view-frustum culling (read-only, cell-level candidates)
-        const core::spatial::SpatialIndex* mSpatialIndex{nullptr};
+        const core::spatial::SpatialIndexV2Flat* mSpatialIndex{nullptr};
+
+        /// O(1) mapping from SpatialId32 -> Entity (index = id).
+        const Entity* mEntitiesBySpatialId{nullptr};
+        std::size_t mEntitiesBySpatialIdSize{0};
 
         /// Resource manager (resident-only access in render; NO lazy-load allowed).
         const core::resources::ResourceManager* mResources{nullptr};
@@ -182,7 +185,7 @@ namespace core::ecs {
         // Буферы (amortized growth, переиспользуются между кадрами)
         // ----------------------------------------------------------------------------------------
 
-        std::vector<Entity> mVisibleEntities;
+        std::vector<core::spatial::EntityId32> mVisibleIds;
         std::vector<RenderKey> mKeys;
         std::vector<RenderPacket> mPackets;
 
diff --git a/SFML1/include/core/ecs/systems/spatial_index_system.h b/SFML1/include/core/ecs/systems/spatial_index_system.h
index 4da6e00..c772489 100644
--- a/SFML1/include/core/ecs/systems/spatial_index_system.h
+++ b/SFML1/include/core/ecs/systems/spatial_index_system.h
@@ -10,22 +10,38 @@
 #include "adapters/entt/entt_registry.hpp"
 
 #include "core/ecs/system.h"
-#include "core/spatial/spatial_index.h"
+#include <cstddef>
+#include <cstdint>
+#include <span>
+#include <vector>
+
+#include "core/spatial/spatial_index_v2.h"
 
 namespace core::ecs {
 
+    struct SpatialIndexSystemConfig final {
+        core::spatial::SpatialIndexConfig index{};
+        core::spatial::FlatStorageConfig storage{};
+        std::uint32_t maxEntityId = 0;
+        bool determinismEnabled = false;
+    };
+
     class SpatialIndexSystem final : public ISystem {
       public:
-        explicit SpatialIndexSystem(float cellSize = 256.f) noexcept;
+        explicit SpatialIndexSystem(const SpatialIndexSystemConfig& config) noexcept;
 
-        [[nodiscard]] core::spatial::SpatialIndex& index() noexcept {
+        [[nodiscard]] core::spatial::SpatialIndexV2Flat& index() noexcept {
             return mIndex;
         }
 
-        [[nodiscard]] const core::spatial::SpatialIndex& index() const noexcept {
+        [[nodiscard]] const core::spatial::SpatialIndexV2Flat& index() const noexcept { 
             return mIndex;
         }
 
+        [[nodiscard]] std::span<const Entity> entitiesBySpatialId() const noexcept {
+            return mEntityBySpatialId;
+        }
+
         void update(World& world, float dt) override;
         void render(World&, sf::RenderWindow&) override {
         }
@@ -35,7 +51,17 @@ namespace core::ecs {
 
         void onHandleDestroyed(entt::registry& registry, Entity entity) noexcept;
 
-        core::spatial::SpatialIndex mIndex;
+        [[nodiscard]] core::spatial::EntityId32 allocateSpatialId();
+        void releaseSpatialId(core::spatial::EntityId32 id) noexcept;
+        void setMapping(core::spatial::EntityId32 id, Entity entity) noexcept;
+
+        core::spatial::SpatialIndexV2Flat mIndex;
+        std::vector<Entity> mEntityBySpatialId{};
+        std::vector<core::spatial::EntityId32> mFreeSpatialIds{};
+        std::vector<Entity> mDirtyScratch{};
+        core::spatial::EntityId32 mNextSpatialId{1};
+        bool mDeterminismEnabled{false};
+        bool mStableIdsPrepared{false};
         entt::scoped_connection mOnDestroyConnection;
         bool mConnected = false;
     };
diff --git a/SFML1/include/core/spatial/spatial_index_v2.h b/SFML1/include/core/spatial/spatial_index_v2.h
index acc65d3..4b63375 100644
--- a/SFML1/include/core/spatial/spatial_index_v2.h
+++ b/SFML1/include/core/spatial/spatial_index_v2.h
@@ -132,6 +132,18 @@ namespace core::spatial {
 
         template <typename Fn> bool forEach(std::uint32_t headHandle, Fn&& fn) const;
 
+#if !defined(NDEBUG) || defined(SFML1_PROFILE)
+        struct DebugStats final {
+            std::uint32_t nodeCapacity = 0;
+            std::uint32_t maxNodes = 0;
+            std::uint32_t peakUsedNodes = 0;
+            std::uint32_t freeDepth = 0;
+        };
+
+        [[nodiscard]] DebugStats debugStats() const noexcept;
+        [[nodiscard]] std::uint32_t totalCount(std::uint32_t headHandle) const noexcept;
+#endif
+
       private:
         [[nodiscard]] std::uint32_t allocateNode();
         void releaseNode(const std::uint32_t handle) noexcept;
@@ -152,6 +164,9 @@ namespace core::spatial {
         std::uint32_t mMaxNodes = 0;
         std::uint32_t mNextUnusedIndex = 0;
         std::uint32_t mFreeCount = 0;
+#if !defined(NDEBUG) || defined(SFML1_PROFILE)
+        std::uint32_t mPeakUsedNodes = 0;
+#endif
 
         std::vector<EntityId32> mEntries{};
         std::vector<std::uint32_t> mNext{};
@@ -371,6 +386,23 @@ namespace core::spatial {
 
         void clearMarksTable() noexcept;
 
+#if !defined(NDEBUG) || defined(SFML1_PROFILE)
+        struct DebugQueryStats final {
+            std::uint32_t cellVisits = 0;
+            std::uint32_t overflowCellVisits = 0;
+            std::uint32_t overflowEntitiesVisited = 0;
+            std::uint32_t candidatesVisited = 0;
+        };
+
+        struct DebugStats final {
+            DebugQueryStats lastQuery{};
+            std::uint32_t worstCellDensity = 0;
+            OverflowPool::DebugStats overflow{};
+        };
+
+        [[nodiscard]] DebugStats debugStats() const noexcept;
+#endif
+
       private:
         struct CellRange final {
             std::int32_t minX = 0;
@@ -436,6 +468,11 @@ namespace core::spatial {
 
         std::int32_t mCellsPerChunkX = 0;
         std::int32_t mCellsPerChunkY = 0;
+
+#if !defined(NDEBUG) || defined(SFML1_PROFILE)
+        mutable DebugQueryStats mDebugLastQueryStats{};
+        std::uint32_t mDebugWorstCellDensity = 0;
+#endif
     };
 
     using SpatialIndexV2Flat = SpatialIndexV2<FlatStorage, Aabb2>;
@@ -456,6 +493,9 @@ namespace core::spatial {
             mFreeStack.clear();
             mFreeCount = 0;
             mNextUnusedIndex = 0;
+#if !defined(NDEBUG) || defined(SFML1_PROFILE)
+            mPeakUsedNodes = 0;
+#endif
             return;
         }
 
@@ -467,6 +507,9 @@ namespace core::spatial {
         mFreeStack.resize(mMaxNodes);
         mFreeCount = 0;
         mNextUnusedIndex = 0;
+#if !defined(NDEBUG) || defined(SFML1_PROFILE)
+        mPeakUsedNodes = 0;
+#endif
     }
 
     inline std::uint32_t OverflowPool::allocateNode() {
@@ -491,6 +534,12 @@ namespace core::spatial {
             mCounts[idx] = 0;
             mNext[idx] = 0;
             return handle;
+#if !defined(NDEBUG) || defined(SFML1_PROFILE)
+            const std::uint32_t used = mNextUnusedIndex - mFreeCount;
+            if (used > mPeakUsedNodes) {
+                mPeakUsedNodes = used;
+            }
+#endif
         }
 
         if (mNextUnusedIndex >= mMaxNodes) {
@@ -500,6 +549,12 @@ namespace core::spatial {
         const std::uint32_t newIndex = mNextUnusedIndex++;
         mCounts[newIndex] = 0;
         mNext[newIndex] = 0;
+#if !defined(NDEBUG) || defined(SFML1_PROFILE)
+        const std::uint32_t used = mNextUnusedIndex - mFreeCount;
+        if (used > mPeakUsedNodes) {
+            mPeakUsedNodes = used;
+        }
+#endif
         return handleFromIndex(newIndex);
     }
 
@@ -697,6 +752,26 @@ namespace core::spatial {
         return true;
     }
 
+#if !defined(NDEBUG) || defined(SFML1_PROFILE)
+    inline OverflowPool::DebugStats OverflowPool::debugStats() const noexcept {
+        return DebugStats{.nodeCapacity = mNodeCapacity,
+                          .maxNodes = mMaxNodes,
+                          .peakUsedNodes = mPeakUsedNodes,
+                          .freeDepth = mFreeCount};
+    }
+
+    inline std::uint32_t OverflowPool::totalCount(const std::uint32_t headHandle) const noexcept {
+        std::uint32_t total = 0;
+        std::uint32_t handle = headHandle;
+        while (handle != 0u) {
+            const std::uint32_t idx = nodeIndex(handle);
+            total += mCounts[idx];
+            handle = mNext[idx];
+        }
+        return total;
+    }
+#endif
+
     inline std::uint32_t FlatStorage::allocateCellsBlock() {
 #if !defined(NDEBUG)
         assert(mFreeBlockCount <= mMaxBlocks && "FlatStorage: freeBlockCount corrupted");
@@ -2125,6 +2200,11 @@ namespace core::spatial {
         if (cell.count < Cell::kInlineCapacity) {
             cell.entities[cell.count] = id;
             ++cell.count;
+#if !defined(NDEBUG) || defined(SFML1_PROFILE)
+            if (cell.count > mDebugWorstCellDensity) {
+                mDebugWorstCellDensity = cell.count;
+            }
+#endif
             return true;
         }
 
@@ -2136,7 +2216,13 @@ namespace core::spatial {
             }
             return false;
         }
-
+#if !defined(NDEBUG) || defined(SFML1_PROFILE)
+        const std::uint32_t overflowCount = mOverflowPool.totalCount(cell.overflowHandle);
+        const std::uint32_t total = static_cast<std::uint32_t>(cell.count) + overflowCount;
+        if (total > mDebugWorstCellDensity) {
+            mDebugWorstCellDensity = total;
+        }
+#endif
         return true;
     }
 
@@ -2282,13 +2368,22 @@ namespace core::spatial {
     inline bool SpatialIndexV2<Storage, BoundsT>::forEachEntityInCell(const Cell& cell,
                                                                       Fn&& fn) const {
         for (std::uint8_t i = 0; i < cell.count; ++i) {
+#if !defined(NDEBUG) || defined(SFML1_PROFILE)
+            ++mDebugLastQueryStats.candidatesVisited;
+#endif
             if (!fn(cell.entities[i])) {
                 return false;
             }
         }
 
         if (cell.overflowHandle != 0u) {
-            return mOverflowPool.forEach(cell.overflowHandle, fn);
+            return mOverflowPool.forEach(cell.overflowHandle, [&](const EntityId32 id) {
+#if !defined(NDEBUG) || defined(SFML1_PROFILE)
+                ++mDebugLastQueryStats.candidatesVisited;
+                ++mDebugLastQueryStats.overflowEntitiesVisited;
+#endif
+                return fn(id);
+            });
         }
 
         return true;
@@ -2345,7 +2440,9 @@ namespace core::spatial {
         if (!beginQueryStamp()) {
             return 0;
         }
-
+#if !defined(NDEBUG) || defined(SFML1_PROFILE)
+        mDebugLastQueryStats = DebugQueryStats{};
+#endif
         const CellRange chunkRange = computeChunkRange(area);
         if (chunkRange.empty()) {
             return 0;
@@ -2368,6 +2465,12 @@ namespace core::spatial {
                     if (stop) {
                         return false;
                     }
+#if !defined(NDEBUG) || defined(SFML1_PROFILE)
+                    ++mDebugLastQueryStats.cellVisits;
+                    if (cell.overflowHandle != 0u) {
+                        ++mDebugLastQueryStats.overflowCellVisits;
+                    }
+#endif
                     return forEachEntityInCell(cell, [&](const EntityId32 id) {
                         if (outCount >= out.size()) {
                             stop = true;
@@ -2397,7 +2500,9 @@ namespace core::spatial {
             handleMarksWrap();
             return 0;
         }
-
+#if !defined(NDEBUG) || defined(SFML1_PROFILE)
+        mDebugLastQueryStats = DebugQueryStats{};
+#endif
         const CellRange chunkRange = computeChunkRange(area);
         if (chunkRange.empty()) {
             return 0;
@@ -2422,6 +2527,12 @@ namespace core::spatial {
                     if (stop) {
                         return false;
                     }
+#if !defined(NDEBUG) || defined(SFML1_PROFILE)
+                    ++mDebugLastQueryStats.cellVisits;
+                    if (cell.overflowHandle != 0u) {
+                        ++mDebugLastQueryStats.overflowCellVisits;
+                    }
+#endif
                     return forEachEntityInCell(cell, [&](const EntityId32 id) {
                         if (outCount >= out.size()) {
                             handleOutputOverflow();
@@ -2444,4 +2555,14 @@ namespace core::spatial {
         return outCount;
     }
 
+#if !defined(NDEBUG) || defined(SFML1_PROFILE)
+    template <typename Storage, typename BoundsT>
+    inline typename SpatialIndexV2<Storage, BoundsT>::DebugStats
+    SpatialIndexV2<Storage, BoundsT>::debugStats() const noexcept {
+        return DebugStats{.lastQuery = mDebugLastQueryStats,
+                          .worstCellDensity = mDebugWorstCellDensity,
+                          .overflow = mOverflowPool.debugStats()};
+    }
+#endif
+
 } // namespace core::spatial
diff --git a/SFML1/src/core/ecs/stable_id_service.cpp b/SFML1/src/core/ecs/stable_id_service.cpp
index 4e2ee15..5b8cfc2 100644
--- a/SFML1/src/core/ecs/stable_id_service.cpp
+++ b/SFML1/src/core/ecs/stable_id_service.cpp
@@ -111,6 +111,30 @@ namespace core::ecs {
         mObservedVersionPlusOneByIndex.clear();
     }
 
+    void StableIdService::prewarm(const std::size_t maxEntities) {
+#if !defined(NDEBUG)
+        debugAssertInvariants();
+        assert(mEnabled && "StableIdService::prewarm: service is disabled. Call enable().");
+#else
+        if (!mEnabled) {
+            LOG_PANIC(core::log::cat::ECS,
+                      "StableIdService::prewarm: service is disabled. Call enable().");
+        }
+#endif
+
+        if (maxEntities == 0) {
+            return;
+        }
+
+        const std::size_t newSize = maxEntities;
+        if (mStableIdByIndex.size() >= newSize) {
+            return;
+        }
+
+        mStableIdByIndex.resize(newSize, kUnsetStableId);
+        mObservedVersionPlusOneByIndex.resize(newSize, kUnsetObservedVersionPlusOne);
+    }
+
     std::size_t StableIdService::rawIndex(const Entity e) noexcept {
         return static_cast<std::size_t>(entt::to_entity(e));
     }
diff --git a/SFML1/src/core/ecs/systems/render_system.cpp b/SFML1/src/core/ecs/systems/render_system.cpp
index 647d429..dfebe6d 100644
--- a/SFML1/src/core/ecs/systems/render_system.cpp
+++ b/SFML1/src/core/ecs/systems/render_system.cpp
@@ -11,14 +11,14 @@
 
 #include <SFML/System/Angle.hpp>
 
-#include "core/ecs/components/spatial_handle_component.h"
+#include "core/ecs/components/spatial_id_component.h"
 #include "core/ecs/components/sprite_component.h"
 #include "core/ecs/components/transform_component.h"
 #include "core/ecs/world.h"
 #include "core/log/log_macros.h"
 #include "core/resources/resource_manager.h" // resident-only access (expectTextureResident)
 #include "core/spatial/aabb2.h"
-#include "core/spatial/spatial_index.h"      // mSpatialIndex->query()
+#include "core/spatial/spatial_index_v2.h"
 #include "core/utils/math_constants.h"
 
 namespace {
@@ -181,10 +181,12 @@ namespace core::ecs {
 #endif
     }
 
-    void RenderSystem::bind(const core::spatial::SpatialIndex* spatialIndex,
+    void RenderSystem::bind(const core::spatial::SpatialIndexV2Flat* spatialIndex,
+                            std::span<const Entity> entitiesBySpatialId,
                             const core::resources::ResourceManager* resources) {
         const bool hasIndex = (spatialIndex != nullptr);
         const bool hasRes = (resources != nullptr);
+        const bool hasMapping = (!entitiesBySpatialId.empty());
 
         // Контракт: либо оба nullptr (unbind), либо оба валидны.
         if (hasIndex != hasRes) {
@@ -194,9 +196,17 @@ namespace core::ecs {
                       static_cast<const void*>(spatialIndex),
                       static_cast<const void*>(resources));
         }
+        if (hasIndex != hasMapping) {
+            LOG_PANIC(core::log::cat::ECS,
+                      "RenderSystem::bind: spatial mapping mismatch (spatialIndex={}, "
+                      "mappingSize={})",
+                      static_cast<const void*>(spatialIndex), entitiesBySpatialId.size());
+        }
 
         mSpatialIndex = spatialIndex;
         mResources = resources;
+        mEntitiesBySpatialId = entitiesBySpatialId.data();
+        mEntitiesBySpatialIdSize = entitiesBySpatialId.size();
 
         // Unbind: bind(nullptr, nullptr).
         if (mResources == nullptr) {
@@ -204,6 +214,9 @@ namespace core::ecs {
             mFrameTextureStamp.clear();
             mFrameId = 0;
             mCachedResourceGen = 0;
+            mVisibleIds.clear();
+            mEntitiesBySpatialId = nullptr;
+            mEntitiesBySpatialIdSize = 0;
 
 #if !defined(NDEBUG) || defined(SFML1_PROFILE)
             mUniqueTexturesThisFrame = 0;
@@ -235,6 +248,28 @@ namespace core::ecs {
 
         resizeEpochArrays(count);
         mCachedResourceGen = mResources->cacheGeneration();
+
+        // Prewarm query buffer to the max SpatialId32 capacity (no hot-path growth).
+        if (mVisibleIds.capacity() < mEntitiesBySpatialIdSize) {
+            mVisibleIds.reserve(mEntitiesBySpatialIdSize);
+        }
+
+        const std::size_t maxVisible =
+            (mEntitiesBySpatialIdSize > 0) ? (mEntitiesBySpatialIdSize - 1u) : 0u;
+        if (maxVisible > 0) {
+            if (mKeys.capacity() < maxVisible) {
+                mKeys.reserve(maxVisible);
+            }
+            if (mPackets.capacity() < maxVisible) {
+                mPackets.reserve(maxVisible);
+            }
+
+            const std::size_t maxVertices = maxVisible * 6u;
+            if (maxVertices > mVertexBufferCapacity) {
+                mVertexBuffer = std::make_unique_for_overwrite<sf::Vertex[]>(maxVertices);
+                mVertexBufferCapacity = maxVertices;
+            }
+        }
     }
 
     void RenderSystem::resizeEpochArrays(const std::size_t textureCount) {
@@ -326,6 +361,8 @@ namespace core::ecs {
                "RenderSystem: view rotation is not supported (view.getRotation() must be 0).");
         assert(mSpatialIndex != nullptr && "RenderSystem: bind() не вызван — mSpatialIndex null");
         assert(mResources != nullptr && "RenderSystem: bind() не вызван — mResources null");
+        assert(mEntitiesBySpatialId != nullptr && mEntitiesBySpatialIdSize > 0 &&
+               "RenderSystem: spatial mapping not bound");
         assert(mFrameTexturePtr.size() == mResources->registry().textureCount() &&
                "RenderSystem: epoch cache size mismatch (call bind() after registry reload).");
         assert(mFrameTextureStamp.size() == mResources->registry().textureCount() &&
@@ -368,16 +405,26 @@ namespace core::ecs {
             }
         };
 
-        ensureCapacity(mVisibleEntities, mLastVisibleCount);
+        if (mVisibleIds.capacity() == 0) {
+            LOG_PANIC(core::log::cat::ECS,
+                      "RenderSystem: query buffer not prewarmed (bind() not called)");
+        }
 
-        mVisibleEntities.clear();
         mKeys.clear();
         mPackets.clear();
         beginFrame();
 
-        mSpatialIndex->query(viewAabb, mVisibleEntities);
+        const std::span<core::spatial::EntityId32> outIds(mVisibleIds.data(),
+                                                          mVisibleIds.capacity());
+        const std::size_t idCount = mSpatialIndex->queryFast(viewAabb, outIds);
+        if (idCount == outIds.size()) {
+            LOG_PANIC(core::log::cat::ECS,
+                      "RenderSystem: SpatialIndexV2 query buffer too small (capacity={})",
+                      outIds.size());
+        }
+        mVisibleIds.resize(idCount);
 
-        const std::size_t visibleCount = mVisibleEntities.size();
+        const std::size_t visibleCount = mVisibleIds.size();
         mLastVisibleCount = visibleCount;
 
         ensureCapacity(mKeys, visibleCount);
@@ -396,21 +443,27 @@ namespace core::ecs {
             mVertexBufferCapacity = newCapacity;
         }
 
-        auto ecsView = world.view<TransformComponent, SpriteComponent, SpatialHandleComponent>();
+        auto ecsView = world.view<TransformComponent, SpriteComponent, SpatialIdComponent>();
 
-        // Итерация по mVisibleEntities (уже отфильтровано SpatialIndex).
+        // Итерация по mVisibleIds (уже отфильтровано SpatialIndexV2).
         // Если число видимых сущностей будет постоянно превышать 50k, поменять на:
         //  packed iteration через view.each() + inline culling.
 
-        for (const Entity entity : mVisibleEntities) {
+        for (const core::spatial::EntityId32 id : mVisibleIds) {
+            assert(id < mEntitiesBySpatialIdSize &&
+                   "RenderSystem: SpatialId32 out of range (mapping)");
+            const Entity entity = mEntitiesBySpatialId[id];
+            if (entity == core::ecs::NullEntity) {
+                continue;
+            }
             assert(ecsView.contains(entity) && "RenderSystem: SpatialIndex вернул entity без "
-                                               "(Transform, Sprite, SpatialHandle)");
+                                               "(Transform, Sprite, SpatialId)");
 #if !defined(NDEBUG) || defined(SFML1_PROFILE)
             ++totalCandidates;
 #endif
             const auto& spr = ecsView.get<SpriteComponent>(entity);
             const auto& tr = ecsView.get<TransformComponent>(entity);
-            const auto& sh = ecsView.get<SpatialHandleComponent>(entity);
+            const auto& sh = ecsView.get<SpatialIdComponent>(entity);
 
 #if !defined(NDEBUG)
             const bool dataFinite = isFiniteVec2(tr.position) &&
diff --git a/SFML1/src/core/ecs/systems/spatial_index_system.cpp b/SFML1/src/core/ecs/systems/spatial_index_system.cpp
index 1ecd61c..6a558eb 100644
--- a/SFML1/src/core/ecs/systems/spatial_index_system.cpp
+++ b/SFML1/src/core/ecs/systems/spatial_index_system.cpp
@@ -6,11 +6,12 @@
 #include <cmath>
 
 #include "core/ecs/components/spatial_dirty_tag.h"
-#include "core/ecs/components/spatial_handle_component.h"
+#include "core/ecs/components/spatial_id_component.h"
 #include "core/ecs/components/sprite_component.h"
 #include "core/ecs/components/transform_component.h"
 #include "core/ecs/render/sprite_bounds.h"
 #include "core/ecs/world.h"
+#include "core/log/log_macros.h"
 #include "core/utils/math_constants.h"
 
 namespace {
@@ -31,8 +32,35 @@ namespace {
 
 namespace core::ecs {
 
-    SpatialIndexSystem::SpatialIndexSystem(float cellSize) noexcept
-        : mIndex(cellSize) {
+    SpatialIndexSystem::SpatialIndexSystem(const SpatialIndexSystemConfig& config) noexcept
+        : mIndex() {
+        assert(config.maxEntityId > 0 && "SpatialIndexSystem: maxEntityId must be > 0");
+        assert(config.index.maxEntityId > 0 && "SpatialIndexSystem: index.maxEntityId must be > 0");
+        assert(config.index.cellSizeWorld > 0 && "SpatialIndexSystem: cellSizeWorld must be > 0");
+        assert(config.index.chunkSizeWorld > 0 && "SpatialIndexSystem: chunkSizeWorld must be > 0");
+        assert(config.index.maxEntityId == config.maxEntityId &&
+               "SpatialIndexSystem: index.maxEntityId must match maxEntityId");
+
+        mIndex.init(config.index, config.storage);
+        const auto origin = config.storage.origin;
+        for (std::int32_t y = origin.y; y < origin.y + config.storage.height; ++y) {
+            for (std::int32_t x = origin.x; x < origin.x + config.storage.width; ++x) {
+                const bool loaded =
+                    mIndex.setChunkState({x, y}, core::spatial::ResidencyState::Loaded);
+                if (!loaded) {
+                    LOG_PANIC(core::log::cat::ECS,
+                              "SpatialIndexSystem: failed to set chunk loaded ({}, {})", x, y);
+                }
+            }
+        }
+
+        mEntityBySpatialId.clear();
+        mEntityBySpatialId.resize(static_cast<std::size_t>(config.maxEntityId) + 1u,
+                                  core::ecs::NullEntity);
+        mFreeSpatialIds.clear();
+        mFreeSpatialIds.reserve(static_cast<std::size_t>(config.maxEntityId) / 16u);
+        mNextSpatialId = 1u;
+        mDeterminismEnabled = config.determinismEnabled;
     }
 
     void SpatialIndexSystem::update(World& world, float) {
@@ -40,8 +68,15 @@ namespace core::ecs {
 
         auto& registry = world.registry();
 
+        if (mDeterminismEnabled && !mStableIdsPrepared) {
+            world.stableIds().enable();
+            world.stableIds().prewarm(mEntityBySpatialId.size());
+            mStableIdsPrepared = true;
+        }
+
         auto newView = registry.view<TransformComponent, SpriteComponent>(
-            entt::exclude<SpatialHandleComponent>);
+            entt::exclude<SpatialIdComponent>);
+
 
         for (auto [entity, transform, sprite] : newView.each()) {
             assert(core::ecs::render::hasExplicitRect(sprite.textureRect) &&
@@ -49,30 +84,100 @@ namespace core::ecs {
 
             const core::spatial::Aabb2 aabb = computeEntityAabb(transform, sprite);
 
-            const std::uint32_t handle = mIndex.registerEntity(entity, aabb);
+            const core::spatial::EntityId32 id = allocateSpatialId();
+            if (id == 0) {
+                LOG_PANIC(core::log::cat::ECS,
+                          "SpatialIndexSystem: SpatialId32 pool exhausted (maxEntityId={})",
+                          mEntityBySpatialId.size() - 1u);
+            }
 
-            // entity гарантированно не имел SpatialHandleComponent (exclude<>),
+            const core::spatial::WriteResult result = mIndex.registerEntity(id, aabb);
+            if (result == core::spatial::WriteResult::Rejected) {
+                releaseSpatialId(id);
+                LOG_PANIC(core::log::cat::ECS,
+                          "SpatialIndexSystem: registerEntity rejected (id={})", id);
+            }
+            if (result == core::spatial::WriteResult::PartialTruncated) {
+                releaseSpatialId(id);
+                LOG_PANIC(core::log::cat::ECS,
+                          "SpatialIndexSystem: registerEntity truncated (id={})", id);
+            }
+
+            // entity гарантированно не имел SpatialIdComponent (exclude<>),
             // поэтому emplace достаточно.
-            registry.emplace<SpatialHandleComponent>(entity, SpatialHandleComponent{handle, aabb});
+            registry.emplace<SpatialIdComponent>(entity, SpatialIdComponent{id, aabb});
+            setMapping(id, entity);
         }
 
-        auto dirtyView = registry.view<SpatialHandleComponent,
+        auto dirtyView = registry.view<SpatialIdComponent,
                                        SpatialDirtyTag,
                                        TransformComponent,
                                        SpriteComponent>();
 
         bool hadDirty = false;
-        for (auto [entity, handleComp, transform, sprite] : dirtyView.each()) {
-            (void)entity;
-            hadDirty = true;
-
-            assert(core::ecs::render::hasExplicitRect(sprite.textureRect) &&
-                   "SpatialIndexSystem: SpriteComponent.textureRect must be explicit");
+        if (mDeterminismEnabled) {
+            if (mDirtyScratch.capacity() < dirtyView.size_hint()) {
+                mDirtyScratch.reserve(dirtyView.size_hint());
+            }
+            mDirtyScratch.clear();
 
-            const core::spatial::Aabb2 newAabb = computeEntityAabb(transform, sprite);
+            for (auto [entity] : dirtyView.each()) {
+                mDirtyScratch.push_back(entity);
+            }
 
-            mIndex.update(handleComp.handle, handleComp.lastAabb, newAabb);
-            handleComp.lastAabb = newAabb;
+            std::sort(mDirtyScratch.begin(), mDirtyScratch.end(),
+                      [&](const Entity a, const Entity b) noexcept {
+                          const auto aId = world.stableIds().ensureAssigned(a);
+                          const auto bId = world.stableIds().ensureAssigned(b);
+                          return aId < bId;
+                      });
+
+            for (const Entity entity : mDirtyScratch) {
+                auto& handleComp = registry.get<SpatialIdComponent>(entity);
+                const auto& transform = registry.get<TransformComponent>(entity);
+                const auto& sprite = registry.get<SpriteComponent>(entity);
+
+                hadDirty = true;
+
+                assert(core::ecs::render::hasExplicitRect(sprite.textureRect) &&
+                       "SpatialIndexSystem: SpriteComponent.textureRect must be explicit");
+
+                const core::spatial::Aabb2 newAabb = computeEntityAabb(transform, sprite);
+
+                const core::spatial::WriteResult result =
+                    mIndex.updateEntity(handleComp.id, newAabb);
+                if (result == core::spatial::WriteResult::Rejected) {
+                    LOG_PANIC(core::log::cat::ECS,
+                              "SpatialIndexSystem: updateEntity rejected (id={})", handleComp.id);
+                }
+                if (result == core::spatial::WriteResult::PartialTruncated) {
+                    LOG_PANIC(core::log::cat::ECS,
+                              "SpatialIndexSystem: updateEntity truncated (id={})", handleComp.id);
+                }
+                handleComp.lastAabb = newAabb;
+            }
+        } else {
+            for (auto [entity, handleComp, transform, sprite] : dirtyView.each()) {
+                (void) entity;
+                hadDirty = true;
+
+                assert(core::ecs::render::hasExplicitRect(sprite.textureRect) &&
+                       "SpatialIndexSystem: SpriteComponent.textureRect must be explicit");
+
+                const core::spatial::Aabb2 newAabb = computeEntityAabb(transform, sprite);
+
+                const core::spatial::WriteResult result =
+                    mIndex.updateEntity(handleComp.id, newAabb);
+                if (result == core::spatial::WriteResult::Rejected) {
+                    LOG_PANIC(core::log::cat::ECS,
+                              "SpatialIndexSystem: updateEntity rejected (id={})", handleComp.id);
+                }
+                if (result == core::spatial::WriteResult::PartialTruncated) {
+                    LOG_PANIC(core::log::cat::ECS,
+                              "SpatialIndexSystem: updateEntity truncated (id={})", handleComp.id);
+                }
+                handleComp.lastAabb = newAabb;
+            }
         }
 
         // ВАЖНО: не удаляем SpatialDirtyTag внутри итерации view (риск инвалидации).
@@ -89,17 +194,53 @@ namespace core::ecs {
         }
 
         auto& registry = world.registry();
-        mOnDestroyConnection = registry.on_destroy<SpatialHandleComponent>().connect<
+        mOnDestroyConnection = registry.on_destroy<SpatialIdComponent>().connect<
             &SpatialIndexSystem::onHandleDestroyed>(*this);
         mConnected = true;
     }
 
     void SpatialIndexSystem::onHandleDestroyed(entt::registry& registry, Entity entity) noexcept {
-        if (const auto* handle = registry.try_get<SpatialHandleComponent>(entity)) {
-            if (handle->handle != 0) {
-                mIndex.unregister(handle->handle, handle->lastAabb);
+        if (const auto* handle = registry.try_get<SpatialIdComponent>(entity)) {
+            if (handle->id != 0) {
+                const bool removed = mIndex.unregisterEntity(handle->id);
+                if (!removed) {
+                    LOG_PANIC(core::log::cat::ECS,
+                              "SpatialIndexSystem: unregisterEntity rejected (id={})", handle->id);
+                }
+                releaseSpatialId(handle->id);
             }
         }
     }
 
+    core::spatial::EntityId32 SpatialIndexSystem::allocateSpatialId() {
+        if (!mFreeSpatialIds.empty()) {
+            const core::spatial::EntityId32 id = mFreeSpatialIds.back();
+            mFreeSpatialIds.pop_back();
+            return id;
+        }
+
+        const core::spatial::EntityId32 id = mNextSpatialId;
+        if (id >= mEntityBySpatialId.size()) {
+            return 0;
+        }
+        ++mNextSpatialId;
+        return id;
+    }
+
+    void SpatialIndexSystem::releaseSpatialId(const core::spatial::EntityId32 id) noexcept {
+        if (id == 0) {
+            return;
+        }
+        if (id < mEntityBySpatialId.size()) {
+            mEntityBySpatialId[id] = core::ecs::NullEntity;
+        }
+        mFreeSpatialIds.push_back(id);
+    }
+
+    void SpatialIndexSystem::setMapping(const core::spatial::EntityId32 id,
+                                        const Entity entity) noexcept {
+        assert(id < mEntityBySpatialId.size() && "SpatialIndexSystem: SpatialId32 out of range");
+        mEntityBySpatialId[id] = entity;
+    }
+
 } // namespace core::ecs
\ No newline at end of file
diff --git a/SFML1/src/game/skyguard/game.cpp b/SFML1/src/game/skyguard/game.cpp
index 31b51a2..3f35a5c 100644
--- a/SFML1/src/game/skyguard/game.cpp
+++ b/SFML1/src/game/skyguard/game.cpp
@@ -4,6 +4,7 @@
 
 #include <array>
 #include <cassert>
+#include <cmath>
 #include <cstdint>
 #include <filesystem>
 #include <optional>
@@ -47,6 +48,9 @@
 #if defined(SFML1_PROFILE)
     #include "game/skyguard/dev/stress_scene.h"
 #endif
+#if !defined(NDEBUG) || defined(SFML1_PROFILE)
+    #include "game/skyguard/dev/spatial_index_harness.h"
+#endif
 
 // Движковые конфиги (Vsync, frame limit и т.п.).
 namespace cfg = ::core::config;
@@ -61,6 +65,72 @@ namespace skycfg_paths = ::game::skyguard::config::paths;
 
 namespace platform = ::game::skyguard::platform;
 
+namespace {
+    [[nodiscard]] core::ecs::SpatialIndexSystemConfig
+    makeSpatialIndexV2ConfigSkyGuard(const core::config::EngineSettings& settings,
+                                     const sf::Vector2f worldLogicalSize) {
+        constexpr std::int32_t kChunkSizeWorld = 4096;
+        constexpr std::uint32_t kExpectedMaxEntities = 2'000'000;
+        constexpr std::uint32_t kMaxActiveChunks = 16;
+
+        const float cellSizeFloat = settings.spatialCellSize;
+        const auto cellSizeWorld = static_cast<std::int32_t>(cellSizeFloat);
+        if (cellSizeWorld <= 0 || static_cast<float>(cellSizeWorld) != cellSizeFloat) {
+            LOG_PANIC(core::log::cat::ECS,
+                      "SpatialIndexV2: spatialCellSize must be a positive integer "
+                      "(value={})",
+                      cellSizeFloat);
+        }
+        if (kChunkSizeWorld % cellSizeWorld != 0) {
+            LOG_PANIC(core::log::cat::ECS,
+                      "SpatialIndexV2: chunkSizeWorld must be divisible by cellSizeWorld "
+                      "(chunkSizeWorld={}, cellSizeWorld={})",
+                      kChunkSizeWorld, cellSizeWorld);
+        }
+
+        if (!(worldLogicalSize.x > 0.f) || !(worldLogicalSize.y > 0.f)) {
+            LOG_PANIC(core::log::cat::ECS,
+                      "SpatialIndexV2: worldLogicalSize must be positive ({}x{})",
+                      worldLogicalSize.x, worldLogicalSize.y);
+        }
+
+        const auto chunksX = static_cast<std::int32_t>(
+            std::ceil(static_cast<double>(worldLogicalSize.x) / kChunkSizeWorld));
+        const auto chunksY = static_cast<std::int32_t>(
+            std::ceil(static_cast<double>(worldLogicalSize.y) / kChunkSizeWorld));
+
+        if (chunksX <= 0 || chunksY <= 0) {
+            LOG_PANIC(core::log::cat::ECS, "SpatialIndexV2: computed chunk grid is invalid ({}x{})",
+                      chunksX, chunksY);
+        }
+
+        const std::uint32_t totalChunks =
+            static_cast<std::uint32_t>(chunksX) * static_cast<std::uint32_t>(chunksY);
+        const std::uint32_t maxResident =
+            (totalChunks < kMaxActiveChunks) ? totalChunks : kMaxActiveChunks;
+
+        core::ecs::SpatialIndexSystemConfig cfg{};
+        cfg.index.chunkSizeWorld = kChunkSizeWorld;
+        cfg.index.cellSizeWorld = cellSizeWorld;
+        cfg.index.maxEntityId = kExpectedMaxEntities;
+        cfg.index.marksCapacity = kExpectedMaxEntities;
+        cfg.index.overflowPolicy = core::spatial::OverflowPolicy::FailFast;
+        cfg.index.overflow = core::spatial::OverflowConfig{
+            // Stage 4: explicit fail-fast overflow policy (no hidden growth).
+            .nodeCapacity = 32u,
+            .maxNodes = 0u};
+
+        cfg.storage.origin = core::spatial::ChunkCoord{0, 0};
+        cfg.storage.width = chunksX;
+        cfg.storage.height = chunksY;
+        cfg.storage.maxResidentChunks = maxResident;
+
+        cfg.maxEntityId = kExpectedMaxEntities;
+        cfg.determinismEnabled = false;
+        return cfg;
+    }
+} // namespace
+
 namespace game::skyguard {
 
     Game::Game() {
@@ -210,16 +280,24 @@ namespace game::skyguard {
             mViewManager.getWorldLogicalSize(),
             playerFloorY);
 
-        auto& spatialSystem =
-            mWorld.addSystem<core::ecs::SpatialIndexSystem>(mEngineSettings.spatialCellSize);
+        const core::ecs::SpatialIndexSystemConfig spatialCfg =
+            makeSpatialIndexV2ConfigSkyGuard(mEngineSettings, mViewManager.getWorldLogicalSize());
+
+        auto& spatialSystem = mWorld.addSystem<core::ecs::SpatialIndexSystem>(spatialCfg);
 
         // 1. Создаем систему рендеринга конструктором по умолчанию (без аргументов)
         auto& renderSys = mWorld.addSystem<core::ecs::RenderSystem>();
         // 2. Привязываем зависимости через bind(). Передаем адреса (&).
-        renderSys.bind(&spatialSystem.index(), &mResources);
+        renderSys.bind(&spatialSystem.index(),
+                       spatialSystem.entitiesBySpatialId(),
+                       &mResources);
         // 3. Сохраняем указатель
         mRenderSystem = &renderSys;
 
+#if !defined(NDEBUG) || defined(SFML1_PROFILE)
+        game::skyguard::dev::tryRunSpatialIndexHarnessFromEnv(spatialCfg);
+#endif
+
         // Эти системы требуют прямого доступа (onResize, onKeyEvent),
         // поэтому сохраняем указатели.
         mDebugOverlay = &mWorld.addSystem<core::ecs::DebugOverlaySystem>();
