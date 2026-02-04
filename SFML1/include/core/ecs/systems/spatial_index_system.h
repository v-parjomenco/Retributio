// ================================================================================================
// File: core/ecs/systems/spatial_index_system.h
// Purpose: Maintains SpatialIndex with dirty-on-write updates
// Used by: World/SystemManager, RenderSystem
// ================================================================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include <entt/signal/sigh.hpp>

#include "adapters/entt/entt_registry.hpp"

#include "core/ecs/entity.h"
#include "core/ecs/system.h"

#include "core/spatial/spatial_index_v2.h"

namespace core::ecs {

    struct SpatialIndexSystemConfig final {
        core::spatial::SpatialIndexConfig index{};
        core::spatial::SlidingWindowConfig storage{};
        std::uint32_t maxEntityId = 0;
        std::size_t maxDirtyEntities = 0;
        std::size_t maxVisibleSprites = 0;
        std::uint32_t maxLoadsPerFrame = 0;
        std::uint32_t maxUnloadsPerFrame = 0;
        std::int32_t hysteresisMarginChunks = 0;
        bool determinismEnabled = false;
    };

    class SpatialIndexSystem final : public ISystem {
      public:
        explicit SpatialIndexSystem(const SpatialIndexSystemConfig& config) noexcept;

        [[nodiscard]] core::spatial::SpatialIndexV2Sliding& index() noexcept {
            return mIndex;
        }

        [[nodiscard]] const core::spatial::SpatialIndexV2Sliding& index() const noexcept { 
            return mIndex;
        }

        [[nodiscard]] std::span<const Entity> entitiesBySpatialId() const noexcept {
            return mEntityBySpatialId;
        }

        void ensureDestroyConnection(World& world);

        void update(World& world, float dt) override;
        void render(World&, sf::RenderWindow&) override {
        }

      private:

        using StableId = std::uint64_t;
        void onHandleDestroyed(entt::registry& registry, Entity entity) noexcept;

        [[nodiscard]] core::spatial::EntityId32 allocateSpatialId();
        void releaseSpatialId(core::spatial::EntityId32 id) noexcept;
        void setMapping(core::spatial::EntityId32 id, Entity entity) noexcept;

        core::spatial::SpatialIndexV2Sliding mIndex;
        std::vector<Entity> mEntityBySpatialId{};
        std::vector<core::spatial::EntityId32> mFreeSpatialIds{};
        std::vector<std::pair<StableId, Entity>> mDirtyStableScratch{};
        core::spatial::EntityId32 mNextSpatialId{1};
        bool mDeterminismEnabled{false};        
        entt::scoped_connection mOnDestroyConnection;
        bool mConnected = false;
    };

} // namespace core::ecs