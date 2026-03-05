// ================================================================================================
// File: core/config/properties/debug_overlay_runtime_properties.h
// Purpose: Data-only runtime properties for debug overlay update/smoothing behavior.
// Used by: DebugOverlayBlueprint, DebugOverlayLoader, DebugOverlaySystem (applyRuntimeProperties)
// ================================================================================================
#pragma once

#include <cstdint>
#include <limits>

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
     *
     * Контракт (validate-on-write):
     *  - Loader обязан клампить значения в допустимый диапазон.
     *  - Runtime системы trust-on-read (без clamp в Release).
     */
    struct DebugOverlayRuntimeProperties {
        static constexpr std::uint32_t kMaxUpdateIntervalMs = 10'000u; // UX-лимит
        static constexpr std::uint8_t kMaxSmoothingShift = 8u;         // UX-лимит

        static_assert(kMaxSmoothingShift <
                          static_cast<std::uint8_t>(std::numeric_limits<std::uint64_t>::digits),
                      "kMaxSmoothingShift must be < 64 to avoid UB in (1ull << shift)");
        static_assert(kMaxUpdateIntervalMs <=
                          static_cast<std::uint32_t>(std::numeric_limits<int>::max()),
                      "kMaxUpdateIntervalMs must fit into int for sf::milliseconds(int)");

        std::uint32_t updateIntervalMs = 100; // ~10 Hz по умолчанию
        std::uint8_t smoothingShift = 3;      // alpha=1/8 по умолчанию
    };

} // namespace core::config::properties