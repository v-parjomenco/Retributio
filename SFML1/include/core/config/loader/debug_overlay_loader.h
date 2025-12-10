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
     *  - Если файла нет / не читается           -> лог + дефолт из DebugOverlayBlueprint.
     *  - Если JSON некорректен или невалиден    -> лог + дефолт из DebugOverlayBlueprint.
     *  - Если отдельные поля отсутствуют        -> дефолт из DebugOverlayBlueprint.
     */
    [[nodiscard]] DebugOverlayBlueprint loadDebugOverlayBlueprint(const std::string& path);

} // namespace core::config