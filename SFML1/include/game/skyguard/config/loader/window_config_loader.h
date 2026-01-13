// ================================================================================================
// File: game/skyguard/config/loader/window_config_loader.h
// Purpose: Load SkyGuard WindowConfig from JSON (non-critical, defaults on any failure).
// Used by: game::skyguard::Game / bootstrap.
// Related headers:
//  - game/skyguard/config/window_config.h
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

#include "game/skyguard/config/window_config.h"

namespace game::skyguard::config {

    /**
     * @brief Загружает конфигурацию окна SkyGuard из JSON.
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

} // namespace game::skyguard::config