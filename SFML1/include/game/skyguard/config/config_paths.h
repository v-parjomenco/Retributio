// ================================================================================================
// File: game/skyguard/config/config_paths.h
// Purpose: Centralized file paths to SkyGuard JSON configs used by game bootstrap.
// Used by: game/skyguard/game.cpp
// Notes:
//  - Not related to config_keys.h (JSON field names).
//  - Lives in game layer: core must not assume assets/ layout.
// ================================================================================================
#pragma once

namespace game::skyguard::config::paths {

    inline constexpr const char* DEBUG_OVERLAY   = "assets/core/config/debug_overlay.json";
    inline constexpr const char* ENGINE_SETTINGS = "assets/core/config/engine_settings.json";
    inline constexpr const char* PLAYER          = "assets/game/skyguard/config/player.json";
    inline constexpr const char* RESOURCES       = "assets/game/skyguard/config/resources.json";
    inline constexpr const char* SKYGUARD_GAME   = "assets/game/skyguard/config/skyguard_game.json";

} // namespace game::skyguard::config::paths