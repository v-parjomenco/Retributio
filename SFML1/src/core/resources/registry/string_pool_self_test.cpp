// ================================================================================================
// File: core/resources/registry/string_pool_self_test.cpp
// Purpose: Minimal self-tests for ResourceKey and StringPool (SFML1_TESTS only).
// ================================================================================================
#include "pch.h"

#if defined(SFML1_TESTS)

static_assert(true, "SFML1_TESTS enabled for self-test TU");

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "core/resources/keys/resource_key.h"
#include "core/resources/keys/stable_key.h"
#include "core/resources/registry/string_pool.h"

namespace core::resources::registry::self_test {

    namespace {

        void testResourceKeyEncoding() {
            using core::resources::ResourceKey;
            using core::resources::TextureKey;
            using core::resources::TextureTag;

            const TextureKey invalid{};
            assert(!invalid.valid());
            assert(invalid.raw == 0u);

            const TextureKey key0 = TextureKey::make(0u, 0u);
            assert(key0.valid());
            assert(key0.index() == 0u);
            assert(key0.generation() == 0u);

            const TextureKey key1 = TextureKey::make(42u, 7u);
            assert(key1.valid());
            assert(key1.index() == 42u);
            assert(key1.generation() == 7u);

            const TextureKey key2 = TextureKey::make(42u, 8u);
            assert(key1 != key2);
            assert(key1 < key2);

            // Boundary: MaxIndex должен корректно кодироваться/декодироваться.
            const TextureKey keyMax = TextureKey::make(ResourceKey<TextureTag>::MaxIndex, 255u);
            assert(keyMax.valid());
            assert(keyMax.index() == ResourceKey<TextureTag>::MaxIndex);
            assert(keyMax.generation() == 255u);
        }

        void testStableKey64() {
            const std::string_view a = "core/textures/player";
            const std::string sameStr(a);

            const std::uint64_t k1 = core::resources::computeStableKey64(a);
            const std::uint64_t k2 = core::resources::computeStableKey64(sameStr);
            assert(k1 == k2);

            const std::uint64_t k3 = core::resources::computeStableKey64("core/textures/player2");
            assert(k1 != k3);
        }

        void testStringPoolDedup() {
            StringPool pool;
            const std::string_view a = pool.intern("alpha");
            const std::string_view b = pool.intern("alpha");

            assert(a == b);
            assert(a.data() == b.data());
        }

        void testStringPoolChunkGrowth() {
            StringPool pool;
            // Почти заполняем чанк, чтобы следующий intern гарантированно заставил выделить новый.
            const std::string payload(StringPool::DefaultChunkSize - 64u, 'x');
            const std::string_view first = pool.intern(payload);
            const std::string snapshot(first.data(), first.size());

            const std::string_view second =
                pool.intern("force_new_chunk_because_previous_has_too_little_space");

            assert(first == snapshot);
            assert(second.data() != nullptr);
        }

        void testStringPoolClearLookup() {
            StringPool pool;
            const std::string_view first = pool.intern("dup");

            pool.clearLookup();

            const std::string_view second = pool.intern("dup");
            assert(first == second);
            assert(first.data() != second.data());
        }

    } // namespace

    void run() {
        testResourceKeyEncoding();
        testStableKey64();
        testStringPoolDedup();
        testStringPoolChunkGrowth();
        testStringPoolClearLookup();
    }

} // namespace core::resources::registry::self_test

namespace {

    struct SelfTestRunner {
        SelfTestRunner() {
            core::resources::registry::self_test::run();
        }
    } gSelfTestRunner;

} // namespace

#endif // defined(SFML1_TESTS)