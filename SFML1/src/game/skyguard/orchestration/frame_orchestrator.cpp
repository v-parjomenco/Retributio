#include "pch.h"

#include "game/skyguard/orchestration/frame_orchestrator.h"

#include "core/ecs/systems/spatial_index_system.h"
#include "core/log/log_macros.h"

namespace game::skyguard::orchestration {

    void FrameOrchestrator::bindSpatialIndexSystem(
        core::ecs::SpatialIndexSystem* const spatialIndexSystem) noexcept {
        mSpatialIndexSystem = spatialIndexSystem;
    }

    void FrameOrchestrator::beginFrameRead() noexcept {
#if !defined(NDEBUG)
        assert(mSpatialIndexSystem != nullptr &&
               "FrameOrchestrator::beginFrameRead: SpatialIndexSystem not bound "
               "(call bindSpatialIndexSystem before game loop)");
#else
        if (mSpatialIndexSystem == nullptr) [[unlikely]] {
            LOG_PANIC(core::log::cat::ECS,
                      "FrameOrchestrator::beginFrameRead: SpatialIndexSystem not bound");
        }
#endif
        mSpatialIndexSystem->beginFrameRead();
    }

} // namespace game::skyguard::orchestration