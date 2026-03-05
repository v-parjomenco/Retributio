// ================================================================================================
// File: dev/stress_chunk_content_provider.h
// Purpose: RETRIBUTIO_PROFILE-only stress content provider for streaming validation.
// ================================================================================================
#pragma once

#include "streaming/chunk_content_provider.h"

#if defined(RETRIBUTIO_PROFILE)

#include <cstddef>
#include <cstdint>
#include <vector>

namespace core::resources {
    class ResourceManager;
} // namespace core::resources

namespace game::atrapacielos::dev {

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

        // ----------------------------------------------------------------------------------------
        // Диагностические геттеры (stress runtime stamp, overlay).
        // Post-clamp truth: значения после валидации/клампинга в конструкторе.
        // ----------------------------------------------------------------------------------------
        [[nodiscard]] std::uint32_t seed() const noexcept { return mSeed; }
        [[nodiscard]] std::size_t textureCount() const noexcept { return mTextures.size(); }
        [[nodiscard]] std::size_t zLayers() const noexcept { return mZLayers; }
        [[nodiscard]] std::size_t entitiesPerChunk() const noexcept { return mEntitiesPerChunk; }
        [[nodiscard]] bool isEnabled() const noexcept { return mEnabled; }

      private:
        std::int32_t mChunkSizeWorld = 0;
        std::size_t mEntitiesPerChunk = 0;
        std::size_t mZLayers = 1;
        std::uint32_t mSeed = 0;
        std::vector<core::resources::TextureKey> mTextures{};
        bool mEnabled = false;
    };

} // namespace game::atrapacielos::dev

#endif