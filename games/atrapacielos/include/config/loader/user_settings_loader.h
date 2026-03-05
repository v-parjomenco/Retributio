// ================================================================================================
// File: config/loader/user_settings_loader.h
// Purpose: User settings loader + atomic save (override shipped defaults).
// Used by: game::atrapacielos::Game (startup override + persistence on window changes).
// Related headers:
//  - config/window_config.h
//  - platform/user_paths.h
// Notes:
//  - Loader is non-critical: missing file -> no overrides.
//  - Parse failures -> one WARN + no overrides (details in DEBUG).
//  - Save is atomic: core::utils::FileLoader::writeTextFileAtomic(...)
//  - No hot-path work here: load/save happens on startup and on window events only.
// ================================================================================================
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>

#include "config/window_config.h"

namespace game::atrapacielos::config {

    struct UserWindowSettings {
        std::optional<WindowMode>    mode;
        std::optional<std::uint32_t> width;   // windowed desired client width
        std::optional<std::uint32_t> height;  // windowed desired client height
    };

    struct UserSettings {
        UserWindowSettings window;
    };

    [[nodiscard]] UserSettings loadUserSettings(const std::filesystem::path& path);

    // false => пытались писать и не смогли (без лог-спама; логировать должен caller один раз).
    // true  => записали или нечего писать (no-op).
    [[nodiscard]] bool saveUserSettingsAtomic(const std::filesystem::path& path,
                                              const UserSettings& settings) noexcept;

    [[nodiscard]] WindowConfig applyUserSettings(const WindowConfig& base,
                                                 const UserSettings& settings);

    // validate-on-write + change detection (для debounce/anti-spam на caller уровне)
    [[nodiscard]] bool setWindowMode(UserSettings& settings, WindowMode mode) noexcept;
    [[nodiscard]] bool setWindowedSize(UserSettings& settings,
                                       std::uint32_t width,
                                       std::uint32_t height) noexcept;

} // namespace game::atrapacielos::config
