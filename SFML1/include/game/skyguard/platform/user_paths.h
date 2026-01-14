// ================================================================================================
// File: game/skyguard/platform/user_paths.h
// Purpose: OS-standard writable user-data paths for settings/config.
// Used by: game::skyguard::Game (loading/saving user settings).
// Related headers: game/skyguard/config/loader/user_settings_loader.h
// Notes:
//  - This is a platform boundary (game-layer): no gameplay logic here.
// ================================================================================================
#pragma once

#include <filesystem>
#include <string_view>

namespace game::skyguard::platform {

    [[nodiscard]] std::filesystem::path getUserConfigDir(std::string_view appId);
    [[nodiscard]] std::filesystem::path getUserSettingsPath(std::string_view appId);

} // namespace game::skyguard::platform
