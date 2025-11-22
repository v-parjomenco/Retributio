// ================================================================================================
// File: core/config/blueprints/debug_overlay_blueprint.h
// Purpose: Data-only debug overlay blueprint (enabled flag + text properties)
// Used by: DebugOverlayLoader, Game
// Related headers: core/config/properties/text_properties.h
// ================================================================================================
#pragma once

#include "core/config/properties/text_properties.h"

namespace core::config {

    /**
     * @brief Высокоуровневая конфигурация (blueprint) для debug overlay.
     *
     * Описывает:
     *  - включён ли overlay (enabled);
     *  - позицию, размер и цвет текста (TextProperties).
     *
     * Никакой логики здесь нет — только данные.
     */
    struct DebugOverlayBlueprint {
        bool enabled = true;
        core::config::properties::TextProperties text{};
    };

} // namespace core::config