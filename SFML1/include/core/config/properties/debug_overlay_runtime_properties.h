// ================================================================================================
// File: core/config/properties/debug_overlay_runtime_properties.h
// Purpose: Data-only runtime properties for debug overlay update/smoothing behavior.
// Used by: DebugOverlayBlueprint, DebugOverlayLoader, DebugOverlaySystem (applyRuntimeProperties)
// ================================================================================================
#pragma once

#include <cstdint>

namespace core::config::properties {

    /**
     * @brief Runtime-настройки поведения debug overlay (не стиль текста).
     *
     * updateIntervalMs:
     *  - 0  => обновлять строку каждый кадр,
     *  - >0 => обновлять не чаще, чем раз в N миллисекунд (убираем "дрожь").
     *
     * smoothingShift:
     *  - EMA без float: alpha = 1 / (2^smoothingShift)
     *  - 0 => сглаживание выключено (показываем raw значение).
     */
    struct DebugOverlayRuntimeProperties {
        std::uint32_t updateIntervalMs = 100; // ~10 Hz по умолчанию
        std::uint8_t  smoothingShift   = 3;   // alpha=1/8 по умолчанию
    };

} // namespace core::config::properties