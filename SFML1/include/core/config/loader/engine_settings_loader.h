// ================================================================================================
// File: core/config/loader/engine_settings_loader.h
// Purpose: Loader for EngineSettings from JSON file (engine_settings.json).
// Used by: Game bootstrap / main, renderer initialization.
// Related headers: core/config/engine_settings.h, core/utils/json/json_utils.h,
//                  core/utils/file_loader.h
// ================================================================================================
#pragma once

#include <string>

#include "core/config/engine_settings.h"

namespace core::config {

    /**
     * @brief Загружает engine_settings.json в EngineSettings.
     *
     * Поведение:
     *  - Если файл отсутствует или не читается:
     *      -> logDebug(...) + возвращаются дефолтные EngineSettings.
     *  - Если JSON некорректен или невалиден:
     *      -> logDebug(...) + возвращаются дефолтные EngineSettings.
     *  - Если отдельные поля отсутствуют:
     *      -> используются значения по умолчанию из EngineSettings.
     *
     * То есть это НЕ критичный конфиг: игра продолжает работать с дефолтами.
     */
    EngineSettings loadEngineSettings(const std::string& path);

} // namespace core::config