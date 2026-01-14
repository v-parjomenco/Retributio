// ================================================================================================
// File: game/skyguard/config/app_config.h
// Purpose: Application identity (stable id + display name) loaded from skyguard_game.json.
// Used by: game::skyguard::Game (user-data path + window title).
// Related headers: game/skyguard/config/loader/app_config_loader.h
// ================================================================================================
#pragma once

#include <string>

namespace game::skyguard::config {

    struct AppConfig {
        // Стабильный идентификатор для user-data пути. Не меняем при ребрендинге.
        std::string id = "skyguard";

        // Отображаемое имя приложения (для заголовка окна).
        std::string displayName = "SkyGuard";
    };

} // namespace game::skyguard::config
