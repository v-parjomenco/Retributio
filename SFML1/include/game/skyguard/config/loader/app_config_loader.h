// ================================================================================================
// File: game/skyguard/config/loader/app_config_loader.h
// Purpose: Load AppConfig from JSON (non-critical, defaults on any failure).
// Used by: game::skyguard::Game.
// Related headers:
//  - game/skyguard/config/app_config.h
//  - core/utils/file_loader.h
//  - core/utils/json/json_document.h
//  - core/utils/json/json_parsers.h
//  - game/skyguard/config/config_keys.h
// ================================================================================================
#pragma once

#include <string>

#include "game/skyguard/config/app_config.h"

namespace game::skyguard::config {

    [[nodiscard]] AppConfig loadAppConfig(const std::string& path);

} // namespace game::skyguard::config
