// ================================================================================================
// File: dev/stress_render_chunk_content_provider.h
// Purpose: Profile-only render stress content provider with two modes:
//          GameLike     — uniform distribution, realistic density gradient.
//          Pathological — stack-based clusters in viewport area, extreme visible density.
//
// Hotspot semantics:
//   Hotspot center = world chunk (0,0) = player start position.
//   This is correct for stationary benchmarks: the player does not move during stress.
//   The streaming window is centered on the player by SpatialStreamingSystem, so the
//   hotspot IS around the camera at rest. If the player moves, density does not follow —
//   this is intentional: render stress measures steady-state, not dynamic rebalancing.
// ================================================================================================
#pragma once

#include "streaming/chunk_content_provider.h"

#if defined(RETRIBUTIO_PROFILE)

#include <cstddef>
#include <cstdint>
#include <vector>

#include "dev/stress_render_options.h"

namespace core::resources {
    class ResourceManager;
} // namespace core::resources

namespace game::atrapacielos::dev {

    class StressRenderChunkContentProvider final : public streaming::IChunkContentProvider {
      public:
        /// @param hysteresisMarginChunks  Streaming window margin (from SpatialIndexSystemConfig).
        ///        Used to predict runtime window origin for accurate zone budget calculation.
        ///        Player starts at chunk (0,0); streaming shifts origin to (-margin, -margin).
        /// @param viewportWorldW/H  Logical camera size in world units (from ViewManager).
        ///        Used by pathological mode to size the dense band within each hotspot chunk.
        ///        NOT hardcoded — comes from runtime config.
        StressRenderChunkContentProvider(core::resources::ResourceManager& resources,
                                         core::resources::TextureKey fallbackTexture,
                                         std::int32_t chunkSizeWorld,
                                         std::int32_t windowWidthChunks,
                                         std::int32_t windowHeightChunks,
                                         std::int32_t hysteresisMarginChunks,
                                         float viewportWorldW,
                                         float viewportWorldH);

        [[nodiscard]] std::size_t maxEntitiesPerChunk() const noexcept override;

        [[nodiscard]] std::size_t
        fillChunkContent(core::spatial::ChunkCoord coord,
                         std::span<streaming::ChunkEntityDesc> out) override;

        // Diagnostic accessors (stamp, overlay).
        [[nodiscard]] std::uint32_t seed() const noexcept { return mSeed; }
        [[nodiscard]] std::size_t textureCount() const noexcept { return mTextures.size(); }
        [[nodiscard]] std::size_t zLayers() const noexcept { return mZLayers; }
        [[nodiscard]] std::size_t totalCount() const noexcept { return mTotalCount; }
        [[nodiscard]] std::size_t visibleCount() const noexcept { return mVisibleCount; }
        [[nodiscard]] std::size_t hotspotRadiusChunks() const noexcept {
            return mHotspotRadiusChunks;
        }
        [[nodiscard]] std::size_t overscanChunks() const noexcept { return mOverscanChunks; }
        [[nodiscard]] std::size_t hotspotPerChunk() const noexcept { return mHotspotPerChunk; }
        [[nodiscard]] std::size_t configuredVisibleDensity() const noexcept {
            return mConfiguredVisibleDensity;
        }
        [[nodiscard]] bool isEnabled() const noexcept { return mEnabled; }
        [[nodiscard]] StressRenderMode renderMode() const noexcept { return mRenderMode; }

      private:
        [[nodiscard]] std::size_t chunkSpawnCount(core::spatial::ChunkCoord coord) const noexcept;

        std::size_t fillGameLike(core::spatial::ChunkCoord coord,
                                 std::span<streaming::ChunkEntityDesc> out,
                                 std::size_t count);

        std::size_t fillPathological(core::spatial::ChunkCoord coord,
                                     std::span<streaming::ChunkEntityDesc> out,
                                     std::size_t count);

        StressRenderMode mRenderMode = StressRenderMode::GameLike;
        std::int32_t mChunkSizeWorld = 0;
        std::int32_t mWindowWidthChunks = 0;
        std::int32_t mWindowHeightChunks = 0;

        std::size_t mMaxEntitiesPerChunk = 0;
        std::size_t mZLayers = 1;
        std::uint32_t mSeed = 1;
        std::size_t mTotalCount = 0;
        std::size_t mVisibleCount = 0;
        std::size_t mHotspotRadiusChunks = 1;
        std::size_t mOverscanChunks = 1;
        std::size_t mHotspotPerChunk = 0;
        std::size_t mOverscanPerChunk = 0;
        std::size_t mFarPerChunk = 0;
        std::size_t mConfiguredVisibleDensity = 0;

        // Pathological stack parameters.
        std::size_t mPathClusterCount = 0;
        std::size_t mPathEntitiesPerCluster = 0;
        float mPathJitterRadius = 4.0f;

        // Viewport hint for pathological placement — from runtime ViewManager, not hardcoded.
        float mViewportHintW = 0.0f;
        float mViewportHintH = 0.0f;

        std::vector<core::resources::TextureKey> mTextures{};
        bool mEnabled = false;
    };

} // namespace game::atrapacielos::dev

#endif
