#include "pch.h"

#include "dev/stress_runtime_stamp.h"

#if defined(RETRIBUTIO_PROFILE)

#include "dev/stress_render_chunk_content_provider.h"
#include "dev/stress_chunk_content_provider.h"
#include "dev/stress_render_options.h"

namespace game::atrapacielos::dev {

    StressRuntimeStamp buildSpatialStressRuntimeStamp(
        const StressChunkContentProvider& provider,
        const std::int32_t configWindowWidth,
        const std::int32_t configWindowHeight) noexcept {
        return StressRuntimeStamp{
            .mode = "Spatial",
            .renderSubMode = {},
            .seed = provider.seed(),
            .entitiesPerChunk = provider.entitiesPerChunk(),
            .texCount = provider.textureCount(),
            .zLayers = provider.zLayers(),
            .windowWidth = configWindowWidth,
            .windowHeight = configWindowHeight,
            .totalCount = 0u,
            .visibleCount = 0u,
            .hotPerChunk = 0u,
            .configuredDensity = 0u,
            .hotspotRadius = 0u,
            .overscan = 0u,
        };
    }

    StressRuntimeStamp buildRenderStressRuntimeStamp(
        const StressRenderChunkContentProvider& provider,
        const std::int32_t configWindowWidth,
        const std::int32_t configWindowHeight) noexcept {

        const std::string_view modeStr =
            (provider.renderMode() == StressRenderMode::Pathological)
                ? "Render/Path"
                : "Render/GL";

        return StressRuntimeStamp{
            .mode = modeStr,
            .renderSubMode = (provider.renderMode() == StressRenderMode::Pathological)
                                 ? "Pathological"
                                 : "GameLike",
            .seed = provider.seed(),
            .entitiesPerChunk = provider.hotspotPerChunk(),
            .texCount = provider.textureCount(),
            .zLayers = provider.zLayers(),
            .windowWidth = configWindowWidth,
            .windowHeight = configWindowHeight,
            .totalCount = provider.totalCount(),
            .visibleCount = provider.visibleCount(),
            .hotPerChunk = provider.hotspotPerChunk(),
            .configuredDensity = provider.configuredVisibleDensity(),
            .hotspotRadius = provider.hotspotRadiusChunks(),
            .overscan = provider.overscanChunks(),
        };
    }

} // namespace game::atrapacielos::dev

#endif // defined(RETRIBUTIO_PROFILE)