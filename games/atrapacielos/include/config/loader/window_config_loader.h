// ================================================================================================
// File: config/loader/window_config_loader.h
// Purpose: Load Atrapacielos WindowConfig from JSON (non-critical, defaults on any failure).
// Used by: game::atrapacielos::Game / bootstrap.
// Related headers:
//  - config/window_config.h
//  - core/utils/file_loader.h
//  - core/utils/json/json_document.h
//  - core/utils/json/json_parsers.h
//  - config/config_keys.h
// Notes:
//  - Read/parse failure -> defaults + one WARN.
//  - Invalid individual fields fall back to defaults; semantic clamps are applied on load.
// ================================================================================================
#pragma once

#include <string>

#include "config/window_config.h"

namespace game::atrapacielos::config {

    /**
     * @brief Загружает конфигурацию окна Atrapacielos из JSON.
     *
     * Поведение:
     *  - Если файл отсутствует или не читается:
     *      -> лог в core::log (Config/WARN) + возвращаются дефолтные WindowConfig.
     *  - Если JSON некорректен или невалиден:
     *      -> parseAndValidateNonCritical(...) вернёт std::nullopt,
     *         в лог уже будут записаны сообщения, а loader вернёт дефолтные WindowConfig.
     *  - Если отдельные поля отсутствуют:
     *      -> используются значения по умолчанию из WindowConfig.
     *
     * Режим окна:
     *  - window.mode: "windowed" | "borderless_fullscreen" | "fullscreen"
     *
     * То есть это НЕ критичный конфиг: игра может стартовать и без него.
     */
    [[nodiscard]] WindowConfig loadWindowConfig(const std::string& path);

} // namespace game::atrapacielos::config