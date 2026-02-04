// ================================================================================================
// File: game/skyguard/dev/stress_chunk_content_provider.h
// Purpose: SFML1_PROFILE-only stress content provider for streaming validation.
// ================================================================================================
#pragma once

#include "game/skyguard/streaming/chunk_content_provider.h"

#if defined(SFML1_PROFILE)

#include <cstddef>
#include <cstdint>
#include <vector>

namespace core::resources {
    class ResourceManager;
} // namespace core::resources

namespace game::skyguard::dev {

    class StressChunkContentProvider final : public streaming::IChunkContentProvider {
      public:
        StressChunkContentProvider(core::resources::ResourceManager& resources,
                                   core::resources::TextureKey fallbackTexture,
                                   std::int32_t chunkSizeWorld);

        [[nodiscard]] std::size_t maxEntitiesPerChunk() const noexcept override;

        [[nodiscard]] std::size_t
        fillChunkContent(core::spatial::ChunkCoord coord,
                         std::span<streaming::ChunkEntityDesc> out) override;

        void onChunkUnloaded(core::spatial::ChunkCoord coord) noexcept override;

      private:
        std::int32_t mChunkSizeWorld = 0;
        std::size_t mEntitiesPerChunk = 0;
        std::size_t mZLayers = 1;
        std::uint32_t mSeed = 0;
        std::vector<core::resources::TextureKey> mTextures{};
        bool mEnabled = false;
    };

} // namespace game::skyguard::dev

#endif
