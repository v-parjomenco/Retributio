// ================================================================================================
// File: dev/stress_render_options.h
// Purpose: ENV-driven options for render stress (Profile builds only).
//          Two render modes: GameLike (realistic world) and Pathological (extreme density).
// ================================================================================================
#pragma once

#if defined(RETRIBUTIO_PROFILE)

#include <cstddef>
#include <cstdint>

namespace game::atrapacielos::dev {

    // --------------------------------------------------------------------------------------------
    // StressMode: top-level mode selector (Spatial vs Render).
    // Mutual exclusion enforced in readStressModeFromEnv().
    // --------------------------------------------------------------------------------------------
    enum class StressMode : std::uint8_t {
        None = 0,
        Spatial,
        Render,
    };

    // --------------------------------------------------------------------------------------------
    // StressRenderMode: sub-mode within Render stress.
    //   GameLike    — realistic large-world + hotspot density. Tests spatial/streaming overhead.
    //   Pathological — extreme visible density via stacks. Tests render pipeline ceiling.
    // --------------------------------------------------------------------------------------------
    enum class StressRenderMode : std::uint8_t {
        GameLike = 0,
        Pathological,
    };

    // --------------------------------------------------------------------------------------------
    // StressRenderOptions: parsed from ATRAPACIELOS_STRESS_RENDER_* env namespace.
    // --------------------------------------------------------------------------------------------
    struct StressRenderOptions final {
        bool enabled = false;
        StressRenderMode renderMode = StressRenderMode::GameLike;

        std::uint32_t seed = 1u;

        // World-scale budgets.
        std::size_t totalCount = 0u;

        // Hotspot geometry.
        std::size_t visibleCount = 0u;
        std::size_t visibleDensity = 0u; // Direct per-chunk override (0 = auto).
        std::size_t hotspotRadiusChunks = 1u;
        std::size_t overscanChunks = 1u;

        // Texture/layer diversity.
        std::size_t zLayers = 5u;

        // Pathological-specific (ignored in GameLike).
        std::size_t pathClusterCount = 0u;       // 0 = auto from density.
        std::size_t pathEntitiesPerCluster = 0u; // 0 = auto.
        float pathJitterRadius = 4.0f;           // World units.
    };

    [[nodiscard]] StressMode readStressModeFromEnv();
    [[nodiscard]] StressRenderOptions readStressRenderOptionsFromEnv() noexcept;
    [[nodiscard]] bool readRenderStressOverlayBackplateEnabled() noexcept;

} // namespace game::atrapacielos::dev

#endif