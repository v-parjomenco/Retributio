// ================================================================================================
// File: config/app_config.h
// Purpose: Application identity (stable id + display name) loaded from atrapacielos.json.
// Used by: game::atrapacielos::Game (user-data path + window title).
// Related headers: config/loader/app_config_loader.h
// ================================================================================================
#pragma once

#include <string>

namespace game::atrapacielos::config {

    struct AppConfig {
        // Стабильный идентификатор для user-data пути.
        std::string id = "atrapacielos";

        // Отображаемое имя приложения (для заголовка окна).
        std::string displayName = "Atrapacielos";
    };

} // namespace game::atrapacielos::config
