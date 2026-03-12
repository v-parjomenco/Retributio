#include "pch.h"

#include "dev/stress_render_options.h"

#if defined(RETRIBUTIO_PROFILE)

#include <array>
#include <limits>
#include <string_view>

#include "core/log/log_macros.h"
#include "dev/stress_env_utils.h"

namespace env = game::atrapacielos::dev::env;

namespace game::atrapacielos::dev {

    StressMode readStressModeFromEnv() {
        const bool spatialEnabled = env::readBool("ATRAPACIELOS_STRESS_SPATIAL_ENABLED");
        const bool renderEnabled = env::readBool("ATRAPACIELOS_STRESS_RENDER_ENABLED");

        if (spatialEnabled && renderEnabled) {
            LOG_PANIC(core::log::cat::Performance,
                      "Stress mode conflict: both ATRAPACIELOS_STRESS_SPATIAL_ENABLED=1 "
                      "and ATRAPACIELOS_STRESS_RENDER_ENABLED=1.");
        }

        if (renderEnabled) {
            return StressMode::Render;
        }
        if (spatialEnabled) {
            return StressMode::Spatial;
        }
        return StressMode::None;
    }

    StressRenderOptions readStressRenderOptionsFromEnv() noexcept {
        // 2^21 = 2,097,152. Hard ceiling for entity IDs in the project.
        constexpr std::size_t kMaxCount = 2'097'152u;

        StressRenderOptions out{};
        out.enabled = env::readBool("ATRAPACIELOS_STRESS_RENDER_ENABLED");

        // Mode: gamelike (default) | pathological.
        // Fail-fast on unknown value to prevent silent misconfig.
        {
            std::array<char, 32> buf{};
            const std::string_view modeStr =
                env::readStringView("ATRAPACIELOS_STRESS_RENDER_MODE", buf);

            if (modeStr.empty() || modeStr == "gamelike" || modeStr == "GameLike") {
                out.renderMode = StressRenderMode::GameLike;
            } else if (modeStr == "pathological" || modeStr == "Pathological") {
                out.renderMode = StressRenderMode::Pathological;
            } else {
                // Unknown mode value — fail-fast in Profile to prevent silent misconfig.
                LOG_PANIC(core::log::cat::Performance,
                          "Unknown ATRAPACIELOS_STRESS_RENDER_MODE='{}'. "
                          "Expected: gamelike | pathological.",
                          modeStr);
            }
        }

        out.seed = static_cast<std::uint32_t>(env::readSize(
            "ATRAPACIELOS_STRESS_RENDER_SEED",
            1u,
            static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())));

        // ========================================================================================
        // Phase 1: Read raw env values (0 = "not set").
        // ========================================================================================
        out.totalCount = env::readSize(
            "ATRAPACIELOS_STRESS_RENDER_TOTAL_COUNT", 0u, kMaxCount);
        out.visibleCount = env::readSize(
            "ATRAPACIELOS_STRESS_RENDER_VISIBLE_COUNT", 0u, kMaxCount);
        out.visibleDensity = env::readSize(
            "ATRAPACIELOS_STRESS_RENDER_VISIBLE_DENSITY", 0u, 131'072u);
        out.hotspotRadiusChunks = env::readSize(
            "ATRAPACIELOS_STRESS_RENDER_HOTSPOT_RADIUS_CHUNKS", 1u, 64u);
        out.overscanChunks = env::readSize(
            "ATRAPACIELOS_STRESS_RENDER_OVERSCAN_CHUNKS", 1u, 64u);
        out.zLayers = env::readSize(
            "ATRAPACIELOS_STRESS_RENDER_Z_LAYERS", 5u, 256u);

        // Pathological-specific.
        out.pathClusterCount = env::readSize(
            "ATRAPACIELOS_STRESS_RENDER_PATHOLOGICAL_CLUSTER_COUNT", 0u, 100'000u);
        out.pathEntitiesPerCluster = env::readSize(
            "ATRAPACIELOS_STRESS_RENDER_PATHOLOGICAL_ENTITIES_PER_CLUSTER", 0u, 100'000u);
        out.pathJitterRadius = static_cast<float>(env::readSize(
            "ATRAPACIELOS_STRESS_RENDER_PATHOLOGICAL_JITTER_RADIUS", 4u, 1024u));

        // ========================================================================================
        // Phase 2: Apply defaults for unset values.
        // IMPORTANT: defaults BEFORE clamps, so explicit user values are never lost.
        // Bug history: old order clamped visibleCount to zero totalCount first,
        //              then set totalCount default, losing explicit visibleCount.
        // ========================================================================================
        if (out.enabled && out.totalCount == 0u) {
            out.totalCount = 1'000'000u;
        }

        if (out.enabled && out.visibleDensity == 0u && out.visibleCount == 0u) {
            out.visibleCount = std::min<std::size_t>(out.totalCount, 120'000u);
        }

        // ========================================================================================
        // Phase 3: Final validation clamps (after defaults applied).
        // ========================================================================================
        if (out.visibleCount > out.totalCount) {
            out.visibleCount = out.totalCount;
        }

        return out;
    }

    bool readRenderStressOverlayBackplateEnabled() noexcept {
        return env::readBool("ATRAPACIELOS_STRESS_RENDER_OVERLAY_BACKPLATE");
    }

} // namespace game::atrapacielos::dev

#endif