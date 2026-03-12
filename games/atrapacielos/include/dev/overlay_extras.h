// ================================================================================================
// File: dev/overlay_extras.h
// Purpose: Populate debug overlay with Atrapacielos-specific lines (Debug + Profile).
// ================================================================================================
#pragma once

#if !defined(NDEBUG) || defined(RETRIBUTIO_PROFILE)

namespace core::ecs {
    class DebugOverlaySystem;
    class SpatialIndexSystem;
    class World;
} // namespace core::ecs

namespace game::atrapacielos::ecs {
    class SpatialStreamingSystem;
} // namespace game::atrapacielos::ecs

namespace game::atrapacielos::presentation {
    class BackgroundRenderer;
    class ViewManager;
} // namespace game::atrapacielos::presentation

#if defined(RETRIBUTIO_PROFILE)
namespace game::atrapacielos::dev {
    struct StressRuntimeStamp;
} // namespace game::atrapacielos::dev
#endif

namespace game::atrapacielos::dev {

    void populateDebugOverlayExtraLines(
        core::ecs::DebugOverlaySystem& overlay,
        core::ecs::World& world,
        const core::ecs::SpatialIndexSystem* spatialIndex,
        const ecs::SpatialStreamingSystem* streamingSystem,
        const presentation::BackgroundRenderer& background,
        const presentation::ViewManager& viewManager
#if defined(RETRIBUTIO_PROFILE)
        , const StressRuntimeStamp* stressStamp
#endif
    );

} // namespace game::atrapacielos::dev

#endif // !defined(NDEBUG) || defined(RETRIBUTIO_PROFILE)