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
     *      -> LOG_WARN(...) (категория Config) + возвращаются дефолтные
     *         EngineSettings (vsyncEnabled, frameLimit из значений по умолчанию структуры).
     *  - Если JSON некорректен или структура невалидна:
     *      -> json_utils::parseAndValidateNonCritical(...) вернёт std::nullopt,
     *         в лог уже будут записаны сообщения о проблеме,
     *         а loader вернёт дефолтные EngineSettings.
     *  - Если отдельные поля отсутствуют или имеют неподходящий тип:
     *      -> используются значения по умолчанию из EngineSettings,
     *         при необходимости пишем предупреждения в лог (категория Config).
     *
     * Таким образом:
     *  - отсутствие файла и битый JSON — НЕ критично, движок работает с дефолтами;
     *  - критичными считаются только те конфиги, которые ломают архитектурные инварианты
     *    (ресурсы, blueprints и т.п.), а EngineSettings — user/dev-тюнинг.
     */

    [[nodiscard]] EngineSettings loadEngineSettings(const std::string& path);

} // namespace core::config