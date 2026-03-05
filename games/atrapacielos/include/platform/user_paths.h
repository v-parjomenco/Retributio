// ================================================================================================
// File: platform/user_paths.h
// Purpose: OS-standard writable user-data paths for settings/config.
// Used by: game::atrapacielos::Game (loading/saving user settings).
// Related headers: config/loader/user_settings_loader.h
// Notes:
//  - This is a platform boundary (game-layer): no gameplay logic here.
// ================================================================================================
#pragma once

#include <filesystem>
#include <string_view>

namespace game::atrapacielos::platform {

    [[nodiscard]] std::filesystem::path getUserConfigDir(std::string_view appId);
    [[nodiscard]] std::filesystem::path getUserSettingsPath(std::string_view appId);

} // namespace game::atrapacielos::platform
