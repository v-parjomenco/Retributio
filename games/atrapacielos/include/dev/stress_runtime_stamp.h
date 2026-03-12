// ================================================================================================
// File: dev/stress_runtime_stamp.h
// Purpose: Self-describing runtime stamp for stress overlay (Profile builds only).
// Used by: Game::initWorld (fill once), overlay_extras (read per-frame).
// ================================================================================================
#pragma once

#if defined(RETRIBUTIO_PROFILE)

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace game::atrapacielos::dev {

    class StressChunkContentProvider;
    class StressRenderChunkContentProvider;

    struct StressRuntimeStamp final {
        std::string_view mode{"Off"};
        std::string_view renderSubMode{}; // "GameLike" / "Pathological" (render only)
        std::uint32_t seed{1u};
        std::size_t entitiesPerChunk{0u};
        std::size_t texCount{1u};
        std::size_t zLayers{1u};
        std::int32_t windowWidth{0};
        std::int32_t windowHeight{0};

        // Render-specific.
        std::size_t totalCount{0u};
        std::size_t visibleCount{0u};
        std::size_t hotPerChunk{0u};           // Computed hotspot density (was visibleDensity).
        std::size_t configuredDensity{0u};     // Raw env VISIBLE_DENSITY (0 = auto).
        std::size_t hotspotRadius{0u};
        std::size_t overscan{0u};
    };

    [[nodiscard]] StressRuntimeStamp buildSpatialStressRuntimeStamp(
        const StressChunkContentProvider& provider,
        std::int32_t configWindowWidth,
        std::int32_t configWindowHeight) noexcept;

    [[nodiscard]] StressRuntimeStamp buildRenderStressRuntimeStamp(
        const StressRenderChunkContentProvider& provider,
        std::int32_t configWindowWidth,
        std::int32_t configWindowHeight) noexcept;

} // namespace game::atrapacielos::dev

#endif // defined(RETRIBUTIO_PROFILE)