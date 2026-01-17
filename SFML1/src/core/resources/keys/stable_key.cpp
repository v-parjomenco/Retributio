#include "pch.h"

#include "core/resources/keys/stable_key.h"
#include <cstdint>
#include <string_view>

#include "adapters/xxhash/xxhash.h"

namespace core::resources {

    std::uint64_t computeStableKey64(std::string_view canonicalKey) noexcept {
        // Seed фиксирован (StableKeySeed). Использовать только на этапе загрузки/тулов.
        const auto hash =
            XXH3_64bits_withSeed(canonicalKey.data(), canonicalKey.size(), StableKeySeed);
        return static_cast<std::uint64_t>(hash);
    }

} // namespace core::resources