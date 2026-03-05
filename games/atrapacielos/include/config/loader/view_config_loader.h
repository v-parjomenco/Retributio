// ================================================================================================
// File: config/loader/view_config_loader.h
// Purpose: Load Atrapacielos ViewConfig from JSON (non-critical, defaults on any failure).
// Used by: game::atrapacielos::Game / bootstrap.
// Related headers:
//  - config/view_config.h
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

#include "config/view_config.h"

namespace game::atrapacielos::config {

    /**
     * @brief Загружает конфигурацию View (логические размеры мира/UI, параметры камеры) из JSON.
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
     * Контракт Atrapacielos:
     *  - cameraCenterYMax НЕ задаётся в JSON и фиксирован:
     *      cameraCenterYMax = worldLogicalSize.y * 0.5f
     *  - Камера не откатывается вниз ниже стартовой точки:
     *      centerY = min(desiredCenterY, cameraCenterYMax)
     *
     * Это НЕ критичный конфиг: игра может стартовать и с дефолтными значениями.
     */
    [[nodiscard]] ViewConfig loadViewConfig(const std::string& path);

} // namespace game::atrapacielos::config