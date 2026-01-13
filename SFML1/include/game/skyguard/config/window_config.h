// ================================================================================================
// File: game/skyguard/config/window_config.h
// Purpose: Game-specific window configuration for SkyGuard (resolution, title, mode).
// Used by: game::skyguard::Game, bootstrap code for SFML window.
// Related headers: game/skyguard/config/loader/window_config_loader.h
// ================================================================================================
#pragma once

#include <cstdint>
#include <string>

namespace game::skyguard::config {

    enum class WindowMode : std::uint8_t {
        Windowed = 0,
        BorderlessFullscreen,
        Fullscreen
    };

    /**
     * @brief Runtime-конфиг окна для конкретной игры SkyGuard.
     *
     * Источник истины:
     *  - дефолты ниже;
     *  - поверх них накрывается skyguard_game.json.
     */
    struct WindowConfig {
        WindowMode mode = WindowMode::BorderlessFullscreen;

        // Для Windowed: желаемый client size 
        // (будет зажат в рабочую область монитора, но с сохранением aspect ratio).
        // Для BorderlessFullscreen/Fullscreen: игнорируется (берём desktop mode).
        std::uint32_t width = 1920;
        std::uint32_t height = 1080;

        std::string title = "SkyGuard (name subject to change)";
    };

} // namespace game::skyguard::config