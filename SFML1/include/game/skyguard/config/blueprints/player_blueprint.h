// ================================================================================================
// File: game/skyguard/config/blueprints/player_blueprint.h
// Purpose: High-level player blueprint assembled from generic properties
// Used by: ConfigLoader, PlayerConfigComponent, PlayerInitSystem
// Related headers: core/config/properties/*.h
// ================================================================================================
#pragma once

#include "core/config/properties/anchor_properties.h"
#include "core/config/properties/controls_properties.h"
#include "core/config/properties/movement_properties.h"
#include "core/config/properties/sprite_properties.h"

namespace core::config::blueprints {

/**
 * @brief Высокоуровневый конфиг сущности игрока.
 *
 * Это не «сырые данные из JSON», а уже собранный blueprint из маленьких
 * кирпичиков properties::*. Его задача — описать:
 *
 *  - какой спрайт использовать и с каким масштабом;
 *  - как быстро и с каким трением игрок двигается;
 *  - как он привязан к экрану/миру и что происходит при resize;
 *  - какими клавишами управляется.
 *
 * Важно: этот blueprint всё ещё относится к конкретной игре (Skyguard),
 * но использует максимально переиспользуемые properties из ядра.
 */
struct PlayerBlueprint {
    core::config::properties::SpriteProperties   sprite;
    core::config::properties::MovementProperties movement;
    core::config::properties::AnchorProperties   anchor;
    core::config::properties::ControlsProperties controls;
};

} // namespace core::config::blueprints