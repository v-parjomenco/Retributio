// ================================================================================================
// File: game/skyguard/orchestration/frame_orchestrator.h
// Role: Game-layer frame phase orchestrator
// Notes:
//  - Keeps Game as a pure conductor (no SpatialIndex marks business logic in game.cpp)
//  - Centralizes per-frame read-phase orchestration entrypoint(s)
//  - Current scope: beginFrameRead() for SpatialIndexV2 marks/stamp maintenance
// ================================================================================================
#pragma once

namespace core::ecs {
    class SpatialIndexSystem;
}

namespace game::skyguard::orchestration {

    class FrameOrchestrator final {
      public:
        void bindSpatialIndexSystem(core::ecs::SpatialIndexSystem* spatialIndexSystem) noexcept;
        void beginFrameRead() noexcept;

      private:
        core::ecs::SpatialIndexSystem* mSpatialIndexSystem{nullptr};
    };

} // namespace game::skyguard::orchestration