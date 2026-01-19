// ================================================================================================
// File: game/skyguard/ecs/systems/player_init_system.h
// Purpose: One-shot system converting PlayerBlueprint -> runtime ECS components (ID-based).
// Used by: Game (systems wiring), World/SystemManager.
// Related headers: game/skyguard/config/blueprints/player_blueprint.h
// ================================================================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/ecs/system.h"

#include "game/skyguard/config/blueprints/player_blueprint.h"

namespace core::ecs {
    class World;
}

namespace core::resources {
    class ResourceManager;
}

namespace game::skyguard::ecs {

    /**
     * @brief One-shot spawn system for player entities.
     *
     * Контракт:
     *  - Выполняется один раз (первый update), затем становится no-op.
     *  - Создаёт сущности игроков из переданных PlayerBlueprint.
     *  - Использует ResourceManager только на этапе инициализации (не hot path).
     */
    class PlayerInitSystem final : public core::ecs::ISystem {
      public:
        PlayerInitSystem(core::resources::ResourceManager& resources,
                         std::vector<game::skyguard::config::blueprints::PlayerBlueprint> players);

        void update(core::ecs::World& world, float dt) override;

      private:
        core::resources::ResourceManager& mResources;
        std::vector<game::skyguard::config::blueprints::PlayerBlueprint> mPlayers;
        bool mHasRun{false};
    };

} // namespace game::skyguard::ecs