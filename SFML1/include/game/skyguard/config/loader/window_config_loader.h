// ================================================================================================
// File: game/skyguard/config/loader/window_config_loader.h
// Purpose: Loader for SkyGuard WindowConfig from JSON (skyguard_game.json).
// Used by: game::skyguard::Game / bootstrap code.
// Related headers: game/skyguard/config/window_config.h,
//                  core/utils/json/json_utils.h, core/utils/file_loader.h
// ================================================================================================
#pragma once

#include <string>

#include "game/skyguard/config/window_config.h"

namespace game::skyguard::config {

    /**
     * @brief Загружает конфигурацию окна SkyGuard из JSON.
     *
     * Поведение:
     *  - Если файл отсутствует или не читается:
     *      -> лог в core::log (Debug/Info) + возвращаются дефолтные WindowConfig.
     *  - Если JSON некорректен или невалиден:
     *      -> лог в core::log (Debug/Warn) + возвращаются дефолтные WindowConfig.
     *  - Если отдельные поля отсутствуют:
     *      -> используются значения по умолчанию из WindowConfig.
     *
     * То есть это НЕ критичный конфиг: игра может стартовать и без него.
     */
    WindowConfig loadWindowConfig(const std::string& path);

} // namespace game::skyguard::config