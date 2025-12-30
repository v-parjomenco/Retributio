// ================================================================================================
// File: game/skyguard/config/config_keys.h
// Purpose: Game-specific JSON keys for SkyGuard configuration.
// Used by: ConfigLoader (player.json), ResourcePaths (resources.json) and other SkyGuard loaders
// Related headers: core/config/config_keys.h
// ================================================================================================
#pragma once

#include "core/config/config_keys.h"

namespace game::skyguard::config::keys {

    // --------------------------------------------------------------------------------------------
    // Ключи для игрового конфига SkyGuard (assets/game/skyguard/config/skyguard_game.json)
    // --------------------------------------------------------------------------------------------
    namespace Game {
        inline constexpr const char* WINDOW_WIDTH = "windowWidth";
        inline constexpr const char* WINDOW_HEIGHT = "windowHeight";
        inline constexpr const char* WINDOW_TITLE = "windowTitle";
    } // namespace Game

    // --------------------------------------------------------------------------------------------
    // Ключи для конфигурации игрока (assets/game/skyguard/config/player.json)
    // --------------------------------------------------------------------------------------------
    namespace Player {
        // Базовые поля делим с движком через Common:
        inline constexpr const char* TEXTURE = core::config::keys::Common::TEXTURE;
        inline constexpr const char* SCALE = core::config::keys::Common::SCALE;
        inline constexpr const char* SPEED = core::config::keys::Common::SPEED;
        inline constexpr const char* ACCELERATION = core::config::keys::Common::ACCELERATION;
        inline constexpr const char* FRICTION = core::config::keys::Common::FRICTION;
        inline constexpr const char* ANCHOR = core::config::keys::Common::ANCHOR;
        inline constexpr const char* START_POSITION = core::config::keys::Common::START_POSITION;
        inline constexpr const char* RESIZE_SCALING = core::config::keys::Common::RESIZE_SCALING;
        inline constexpr const char* LOCK_BEHAVIOR = core::config::keys::Common::LOCK_BEHAVIOR;

        inline constexpr const char* CONTROLS = "controls";
        inline constexpr const char* CONTROL_UP = "up";
        inline constexpr const char* CONTROL_DOWN = "down";
        inline constexpr const char* CONTROL_LEFT = "left";
        inline constexpr const char* CONTROL_RIGHT = "right";

        inline constexpr const char* TURN_RATE = "turn_rate";
        inline constexpr const char* INITIAL_ROTATION_DEGREES = "initial_rotation_degrees";
    } // namespace Player

} // namespace game::skyguard::config::keys