// ================================================================================================
// File: config/window_config.h
// Purpose: Game-specific window configuration for Atrapacielos (resolution, mode).
// Used by: game::atrapacielos::Game, bootstrap code for SFML window.
// Related headers: config/loader/window_config_loader.h
// Notes:
//  - Window title is NOT part of WindowConfig. It lives in AppConfig (app.display_name).
// ================================================================================================
#pragma once

#include <cstdint>

namespace game::atrapacielos::config {

    enum class WindowMode : std::uint8_t {
        Windowed = 0,
        BorderlessFullscreen,
        Fullscreen
    };

    /**
     * @brief Runtime-конфиг окна (только то, что относится к режиму/размеру).
     *
     * Источник истины:
     *  - дефолты ниже;
     *  - поверх них накрывается atrapacielos.json (window.*);
     *  - поверх него накрывается user_settings.json (override).
     *
     * width/height — "desired client size" для Windowed.
     * Для BorderlessFullscreen/Fullscreen игнорируются при создании, но используются как
     * размер восстановления при возвращении в Windowed.
     */
    struct WindowConfig {
        WindowMode mode = WindowMode::BorderlessFullscreen;
        std::uint32_t width = 1920;
        std::uint32_t height = 1080;
    };

} // namespace game::atrapacielos::config