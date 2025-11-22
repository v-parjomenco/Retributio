// ================================================================================================
// File: game/skyguard/ecs/components/player_config_component.h
// Purpose: Temporary ECS component storing PlayerBlueprint for initialization
// Used by: Game (initWorld), PlayerInitSystem
// Related headers: game/skyguard/config/blueprints/player_blueprint.h
// ================================================================================================
#pragma once

#include "game/skyguard/config/blueprints/player_blueprint.h"

namespace game::skyguard::ecs {

    /**
     * @brief Компонент конфигурации игрока (blueprint, загружается из JSON).
     *
     * Хранит копию game::skyguard::config::blueprints::PlayerBlueprint, загруженную
     * из JSON через game::skyguard::config::ConfigLoader.
     *
     * Жизненный цикл:
     *  - Game создаёт сущность и добавляет PlayerConfigComponent с blueprint'ом;
     *  - PlayerInitSystem при первом апдейте читает blueprint, создаёт все
     *    необходимые ECS-компоненты (Sprite, Transform, Movement, Controls и т.д.);
     *  - после инициализации PlayerInitSystem удаляет этот компонент.
     */
    struct PlayerConfigComponent {
        game::skyguard::config::blueprints::PlayerBlueprint config;
    };

} // namespace game::skyguard::ecs