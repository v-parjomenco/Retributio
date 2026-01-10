// ================================================================================================
// File: game/skyguard/config/loader/view_config_loader.h
// Purpose: Load SkyGuard ViewConfig from JSON (non-critical, defaults on any failure).
// Used by: game::skyguard::Game / bootstrap.
// Related headers:
//  - game/skyguard/config/view_config.h
//  - core/utils/file_loader.h
//  - core/utils/json/json_document.h
//  - core/utils/json/json_parsers.h
//  - game/skyguard/config/config_keys.h
// Notes:
//  - Read/parse failure -> defaults + one WARN.
//  - Invalid individual fields fall back to defaults; semantic clamps are applied on load.
// ================================================================================================
#pragma once

#include <string>

#include "game/skyguard/config/view_config.h"

namespace game::skyguard::config {

    /**
     * @brief Загружает конфигурацию View (world/ui logical sizes, camera) из JSON.
     *
     * Поведение:
     *  - Если файл отсутствует или не читается:
     *      -> WARN + возвращаются дефолтные ViewConfig.
     *  - Если JSON некорректен или невалиден:
     *      -> parseAndValidateNonCritical(...) вернёт std::nullopt,
     *         в лог уже будут записаны сообщения, а loader вернёт дефолтные ViewConfig.
     *  - Если отдельные поля отсутствуют:
     *      -> используются значения по умолчанию из ViewConfig.
     *
     * Это НЕ критичный конфиг: игра может стартовать и с дефолтными значениями.
     */
    [[nodiscard]] ViewConfig loadViewConfig(const std::string& path);

} // namespace game::skyguard::config