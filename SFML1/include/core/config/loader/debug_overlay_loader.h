// ================================================================================================
// File: core/config/loader/debug_overlay_loader.h
// Purpose: Loader function for DebugOverlayBlueprint from JSON
// Used by: Game
// Related headers: core/config/blueprints/debug_overlay_blueprint.h
// ================================================================================================
#pragma once

#include <string>

#include "core/config/blueprints/debug_overlay_blueprint.h"

namespace core::config {

    /**
     * @brief Загружает JSON из файла и парсит данные в DebugOverlayBlueprint.
     *
     * Поведение:
     *  - Если файл отсутствует или не читается:
     *      -> FileLoader логирует низкоуровневую I/O-проблему (Engine/DEBUG),
     *         loader логирует высокоуровневый контекст (Config/WARN)
     *         и возвращает дефолтный DebugOverlayBlueprint.
     *  - Если JSON некорректен или структура невалидна:
     *      -> json_utils::parseAndValidateNonCritical(...) логирует в Config
     *         и возвращает std::nullopt, loader в этом случае
     *         возвращает дефолтный DebugOverlayBlueprint.
     *  - Если отдельные поля отсутствуют или имеют неверный тип:
     *      -> используются значения по умолчанию из DebugOverlayBlueprint
     *         без падения игры.
     */
    [[nodiscard]] DebugOverlayBlueprint loadDebugOverlayBlueprint(const std::string& path);

} // namespace core::config