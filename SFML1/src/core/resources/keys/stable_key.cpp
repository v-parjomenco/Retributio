#include "pch.h"

// Публичная точка входа — делегирует в link-time seam detail::hashCanonicalKey.
// Реализация seam'а выбирается линкером: production (xxhash) или тест (injectable).
#include "core/resources/keys/stable_key.h"

namespace core::resources {

    std::uint64_t computeStableKey64(std::string_view canonicalKey) noexcept {
        return detail::hashCanonicalKey(canonicalKey);
    }

} // namespace core::resources