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
        inline constexpr const char* APP = "app";
        inline constexpr const char* WINDOW = "window";
        inline constexpr const char* VIEW = "view";
    } // namespace Game

    namespace App {
        inline constexpr const char* ID = "id";
        inline constexpr const char* DISPLAY_NAME = "display_name";
    } // namespace App

    namespace Window {
        inline constexpr const char* MODE = "mode";
        inline constexpr const char* WIDTH = "width";
        inline constexpr const char* HEIGHT = "height";
    } // namespace Window

    namespace View {
        inline constexpr const char* WORLD_LOGICAL_SIZE = "world_logical_size";
        inline constexpr const char* UI_LOGICAL_SIZE = "ui_logical_size";
        inline constexpr const char* CAMERA_OFFSET = "camera_offset";
    } // namespace View

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
        inline constexpr const char* START_POSITION = core::config::keys::Common::START_POSITION;

        inline constexpr const char* CONTROLS = "controls";
        inline constexpr const char* CONTROL_THRUST_FORWARD = "thrust_forward";
        inline constexpr const char* CONTROL_THRUST_BACKWARD = "thrust_backward";
        inline constexpr const char* CONTROL_TURN_LEFT = "turn_left";
        inline constexpr const char* CONTROL_TURN_RIGHT = "turn_right";

        inline constexpr const char* TURN_RATE = "turn_rate";
        inline constexpr const char* INITIAL_ROTATION_DEGREES = "initial_rotation_degrees";
    } // namespace Player

} // namespace game::skyguard::config::keys