// ================================================================================================
// File: core/ecs/systems/spatial_index_system.h
// Purpose: Maintains SpatialIndex with dirty-on-write updates
// Used by: World/SystemManager, RenderSystem
// ================================================================================================
#pragma once

#include <entt/signal/sigh.hpp>

#include "adapters/entt/entt_registry.hpp"

#include "core/ecs/system.h"
#include "core/spatial/spatial_index.h"

namespace core::ecs {

    class SpatialIndexSystem final : public ISystem {
      public:
        explicit SpatialIndexSystem(float cellSize = 256.f) noexcept;

        [[nodiscard]] core::spatial::SpatialIndex& index() noexcept {
            return mIndex;
        }

        [[nodiscard]] const core::spatial::SpatialIndex& index() const noexcept {
            return mIndex;
        }

        void update(World& world, float dt) override;
        void render(World&, sf::RenderWindow&) override {
        }

      private:
        void ensureDestroyConnection(World& world);

        void onHandleDestroyed(entt::registry& registry, Entity entity) noexcept;

        core::spatial::SpatialIndex mIndex;
        entt::scoped_connection mOnDestroyConnection;
        bool mConnected = false;
    };

} // namespace core::ecs