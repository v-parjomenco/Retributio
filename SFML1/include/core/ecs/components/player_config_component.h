// ================================================================================================
// File: core/ecs/components/player_config_component.h
// Purpose: Temporary ECS component storing PlayerBlueprint for initialization
// Used by: Game (initWorld), PlayerInitSystem
// Related headers: core/config/blueprints/player_blueprint.h
// ================================================================================================
#pragma once

#include "game/skyguard/config/blueprints/player_blueprint.h"
#include "core/config/config_loader.h"

namespace core::ecs {

    /**
     * @brief Компонент конфигурации игрока (blueprint, загружается из JSON).
     *
     * Хранит копию core::config::blueprints::PlayerBlueprint, загруженную
     * из JSON через core::config::ConfigLoader.
     *
     * Жизненный цикл:
     *  - Game создаёт сущность и добавляет PlayerConfigComponent с blueprint'ом;
     *  - PlayerInitSystem при первом апдейте читает blueprint, создаёт все
     *    необходимые ECS-компоненты (Sprite, Transform, Movement, Controls и т.д.);
     *  - после инициализации PlayerInitSystem удаляет этот компонент.
     */
    struct PlayerConfigComponent {
        core::config::blueprints::PlayerBlueprint config;
    };

} // namespace core::ecs