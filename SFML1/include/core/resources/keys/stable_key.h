// ================================================================================================
// File: core/resources/keys/stable_key.h
// Purpose: Stable resource key computation (xxHash3_64).
// ================================================================================================
#pragma once

#include <cstdint>
#include <string_view>

namespace core::resources {

    // Контракт: seed фиксирован (0) и НИКОГДА не меняется.
    // Это часть публичного контракта (аудит/совместимость сейвов/модов).
    inline constexpr std::uint64_t StableKeySeed = 0u;

    [[nodiscard]] std::uint64_t computeStableKey64(std::string_view canonicalKey) noexcept;

} // namespace core::resources