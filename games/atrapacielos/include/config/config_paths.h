// ================================================================================================
// File: config/config_paths.h
// Purpose: Centralized file paths to Atrapacielos JSON configs used by game bootstrap.
// Used by: game.cpp
// Notes:
//  - Not related to config_keys.h (JSON field names).
//  - Lives in game layer: core must not assume assets/ layout.
// ================================================================================================
#pragma once

namespace game::atrapacielos::config::paths {

    inline constexpr const char* DEBUG_OVERLAY   = "assets/config/debug_overlay.json";
    inline constexpr const char* ENGINE_SETTINGS = "assets/config/engine_settings.json";
    inline constexpr const char* PLAYER          = "assets/config/player.json";
    inline constexpr const char* RESOURCES       = "assets/config/resources.json";
    inline constexpr const char* ATRAPACIELOS_GAME   = "assets/config/atrapacielos.json";

} // namespace game::atrapacielos::config::paths