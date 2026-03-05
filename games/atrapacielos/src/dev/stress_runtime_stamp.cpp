#include "pch.h"

#include "dev/stress_runtime_stamp.h"

#if defined(RETRIBUTIO_PROFILE)

#include "dev/stress_chunk_content_provider.h"

namespace game::atrapacielos::dev {

    StressRuntimeStamp buildStressRuntimeStamp(
        const StressChunkContentProvider& provider,
        const std::int32_t configWindowWidth,
        const std::int32_t configWindowHeight) noexcept {

        // Все значения — post-clamp truth из фактических runtime-объектов.
        // Ни одного getenv(). Stamp гарантированно соответствует реальному поведению.
        return StressRuntimeStamp{
            .mode = "ActiveSet",
            .seed = provider.seed(),
            .entitiesPerChunk = provider.entitiesPerChunk(),
            .texCount = provider.textureCount(),
            .zLayers = provider.zLayers(),
            .windowWidth = configWindowWidth,
            .windowHeight = configWindowHeight,
        };
    }

} // namespace game::atrapacielos::dev

#endif // defined(RETRIBUTIO_PROFILE)