// ================================================================================================
// File: core/resources/keys/stable_key.h
// Purpose: Stable resource key computation (xxHash3_64).
// ================================================================================================
#pragma once

#include <cstdint>
#include <string_view>

namespace core::resources {

    [[nodiscard]] std::uint64_t computeStableKey64(std::string_view canonicalKey) noexcept;

} // namespace core::resources