#include "pch.h"

#include "adapters/xxhash/xxhash.h"
#include "core/resources/keys/stable_key.h"

namespace core::resources::detail {

    std::uint64_t hashCanonicalKey(std::string_view key) noexcept {
        return static_cast<std::uint64_t>(
            XXH3_64bits_withSeed(key.data(), key.size(), StableKeySeed));
    }

} // namespace core::resources::detail