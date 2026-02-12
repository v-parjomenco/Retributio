#include "pch.h"

#include "game/skyguard/dev/stress_runtime_stamp.h"

#if defined(SFML1_PROFILE)

#include "game/skyguard/dev/stress_chunk_content_provider.h"

namespace game::skyguard::dev {

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

} // namespace game::skyguard::dev

#endif // defined(SFML1_PROFILE)