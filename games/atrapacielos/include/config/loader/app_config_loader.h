// ================================================================================================
// File: config/loader/app_config_loader.h
// Purpose: Load AppConfig from JSON (non-critical, defaults on any failure).
// Used by: game::atrapacielos::Game.
// Related headers:
//  - config/app_config.h
//  - core/utils/file_loader.h
//  - core/utils/json/json_document.h
//  - core/utils/json/json_parsers.h
//  - config/config_keys.h
// ================================================================================================
#pragma once

#include <string>

#include "config/app_config.h"

namespace game::atrapacielos::config {

    [[nodiscard]] AppConfig loadAppConfig(const std::string& path);

} // namespace game::atrapacielos::config
