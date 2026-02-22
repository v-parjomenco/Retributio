#include "core/resources/keys/stable_key.h"
#include "tests/engine/include/stable_key_seam.h"

#include "adapters/xxhash/xxhash.h"

#include <atomic>
#include <cstdint>
#include <string_view>

// Тестовая реализация core::resources::detail::hashCanonicalKey.
// Резолвирует тот же link-seam символ, что и stable_key_impl_xxhash.cpp,
// но дополнительно экспонирует setHashOverride() для инжекции константного хэша.
// Когда override не установлен — поведение идентично production (xxHash3_64).
//
// Линкуется ТОЛЬКО в sfml1_engine_tests. Совместная линковка со
// stable_key_impl_xxhash.cpp приводит к ошибке multiple-definition.
namespace core::resources::detail {

    namespace {

        // seq_cst: override записывается один раз до теста и читается внутри
        // loadFromSources. seq_cst даёт максимальные гарантии порядка и исключает
        // любые вопросы видимости при смешанных load/store из разных потоков.
        std::atomic<HashOverrideFn> g_hashOverride{nullptr};

    } // namespace

    void setHashOverride(HashOverrideFn fn) noexcept {
        g_hashOverride.store(fn, std::memory_order_seq_cst);
    }

    std::uint64_t hashCanonicalKey(std::string_view key) noexcept {
        const HashOverrideFn fn = g_hashOverride.load(std::memory_order_seq_cst);
        if (fn != nullptr) {
            return fn(key);
        }
        return static_cast<std::uint64_t>(
            XXH3_64bits_withSeed(key.data(), key.size(), StableKeySeed));
    }

} // namespace core::resources::detail