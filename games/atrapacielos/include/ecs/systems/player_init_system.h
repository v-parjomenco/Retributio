// ================================================================================================
// File: ecs/systems/player_init_system.h
// Purpose: One-shot system converting PlayerBlueprint -> runtime ECS components (ID-based).
// Used by: Game (systems wiring), World/SystemManager.
// Related headers: config/blueprints/player_blueprint.h
// ================================================================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/ecs/system.h"

#include "config/blueprints/player_blueprint.h"

namespace core::ecs {
    class World;
}

namespace game::atrapacielos::ecs {

    /**
     * @brief One-shot spawn system for player entities.
     *
     * Контракт:
     *  - Выполняется один раз (первый update), затем становится no-op.
     *  - Создаёт сущности игроков из переданных PlayerBlueprint.
     *  - Строго data-only: НЕ зависит от ResourceManager и НЕ трогает SFML Texture.
     *    Все derived sprite данные должны быть заполнены в scene bootstrap (validate-on-write).
     */
    class PlayerInitSystem final : public core::ecs::ISystem {
      public:
        explicit PlayerInitSystem(
            std::vector<game::atrapacielos::config::blueprints::PlayerBlueprint> players);

        void update(core::ecs::World& world, float dt) override;

      private:
        std::vector<game::atrapacielos::config::blueprints::PlayerBlueprint> mPlayers;
        bool mHasRun{false};
    };

} // namespace game::atrapacielos::ecs
