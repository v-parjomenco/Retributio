// ================================================================================================
// File: game/skyguard/dev/stress_runtime_stamp.h
// Purpose: Self-describing runtime stamp for stress overlay (Profile builds only)
// Used by: Game::initWorld (fill once), overlay_extras (read per-frame)
// ================================================================================================
#pragma once

#if defined(SFML1_PROFILE)

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace game::skyguard::dev {

    class StressChunkContentProvider;

    /// Snapshot ключевых параметров stress-прогона для отображения в overlay.
    /// Заполняется ОДИН РАЗ в initWorld() → хранится как Game::mStressStamp.
    /// В overlay читается per-frame — trust-on-read, zero overhead.
    ///
    /// Все значения — post-clamp truth из фактических runtime-объектов:
    ///  - seed, entitiesPerChunk, texCount, zLayers: из StressChunkContentProvider.
    ///  - windowWidth, windowHeight: из SpatialIndexSystemConfig.
    struct StressRuntimeStamp final {
        std::string_view mode{"ActiveSet"};
        std::uint32_t seed{1u};
        std::size_t entitiesPerChunk{0u};
        std::size_t texCount{1u};
        std::size_t zLayers{1u};
        std::int32_t windowWidth{0};
        std::int32_t windowHeight{0};
    };

    /// Собрать stamp из фактических runtime-данных.
    ///
    /// @param provider       StressChunkContentProvider (post-clamp truth для content params).
    /// @param configWindowWidth   spatialCfg.storage.width  (post-clamp truth).
    /// @param configWindowHeight  spatialCfg.storage.height (post-clamp truth).
    ///
    /// Zero ENV reads: единственный source of truth — runtime объекты.
    [[nodiscard]] StressRuntimeStamp buildStressRuntimeStamp(
        const StressChunkContentProvider& provider,
        std::int32_t configWindowWidth,
        std::int32_t configWindowHeight) noexcept;

} // namespace game::skyguard::dev

#endif // defined(SFML1_PROFILE)