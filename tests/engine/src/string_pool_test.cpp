// ================================================================================================
// File: tests/engine/src/string_pool_test.cpp
// Purpose: Google Tests for ResourceKey<T> encoding/decoding, StableKey64, and StringPool.
// Used by: sfml1_engine_tests (CTest, Debug config only)
// Related headers: resource_key.h, stable_key.h, string_pool.h
// Notes:
//  - Migrated from string_pool_self_test.cpp (SFML1_TESTS / static initializer pattern).
//  - No PCH — all includes are explicit.
//  - StableKey64 golden values: xxHash3 64-bit, seed=0, input bytes of key (no null terminator).
//    Strings use canonical dot-format (core.category.name) matching real registry contract.
//    Any change to algorithm/seed/byte-order is a breaking change for saves/mods/replays.
// ================================================================================================

#include <cstdint>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "core/resources/keys/resource_key.h"
#include "core/resources/keys/stable_key.h"
#include "core/resources/registry/string_pool.h"

namespace {

    // ============================================================================================
    // ResourceKey<T> — кодирование / декодирование
    // ============================================================================================

    TEST(ResourceKeyTest, InvalidKey_NotValidAndZeroRawValue) {
        const core::resources::TextureKey invalid{};
        EXPECT_FALSE(invalid.valid());
        EXPECT_EQ(invalid.rawValue(), 0u);
    }

    TEST(ResourceKeyTest, MakeKey_Zero_ValidWithCorrectFields) {
        const auto key = core::resources::TextureKey::make(0u, 0u);
        EXPECT_TRUE(key.valid());
        EXPECT_EQ(key.index(),      0u);
        EXPECT_EQ(key.generation(), 0u);
    }

    TEST(ResourceKeyTest, MakeKey_ArbitraryValues_RoundTrip) {
        const auto key = core::resources::TextureKey::make(42u, 7u);
        EXPECT_TRUE(key.valid());
        EXPECT_EQ(key.index(),      42u);
        EXPECT_EQ(key.generation(), 7u);
    }

    TEST(ResourceKeyTest, SameIndex_DifferentGeneration_NotEqualAndOrdered) {
        const auto key1 = core::resources::TextureKey::make(42u, 7u);
        const auto key2 = core::resources::TextureKey::make(42u, 8u);
        EXPECT_NE(key1, key2);
        EXPECT_LT(key1, key2);
    }

    // Тест граничного значения индекса через TextureKey::MaxIndex.
    // MaxIndex = IndexMask - 1 = 0x00FFFFFE. Если кодировка сломается —
    // index() вернёт неверное значение или assert упадёт в make().
    TEST(ResourceKeyTest, MaxIndex_RoundTrip) {
        using Key = core::resources::TextureKey;

        const auto keyMax = Key::make(Key::MaxIndex, 255u);
        EXPECT_TRUE(keyMax.valid());
        EXPECT_EQ(keyMax.index(),      Key::MaxIndex);
        EXPECT_EQ(keyMax.generation(), 255u);
    }

    // ============================================================================================
    // computeStableKey64 — golden-тесты
    //
    // Жёстко фиксируем ожидаемые значения хэша для реальных canonical keys (формат с точками).
    // Любое изменение алгоритма, seed'а или байтового порядка — breaking change:
    // ломает совместимость сохранений/модов/реплеев.
    //
    // Как верифицировать перед первым коммитом:
    //   printf("%llx\n", XXH3_64bits_withSeed("core.texture.player", 19, 0));
    // ============================================================================================

    TEST(StableKey64Test, Golden_CoreTexturePlayer) {
        // Эталон: xxHash3("core.texture.player", seed=0) = 0x1da85de10268ce71
        constexpr std::uint64_t kExpected = 0x1da85de10268ce71ull;
        EXPECT_EQ(core::resources::computeStableKey64("core.texture.player"), kExpected);
    }

    TEST(StableKey64Test, Golden_CoreTexturePlayer2) {
        // Эталон: xxHash3("core.texture.player2", seed=0) = 0x40597f5aa322a501
        constexpr std::uint64_t kExpected = 0x40597f5aa322a501ull;
        EXPECT_EQ(core::resources::computeStableKey64("core.texture.player2"), kExpected);
    }

    TEST(StableKey64Test, SameInput_StringView_And_String_ProduceSameHash) {
        // string_view и std::string с одинаковым содержимым → одинаковый результат.
        // Проверяет отсутствие зависимости от нулевого терминатора или типа обёртки.
        const std::string_view sv  = "core.texture.player";
        const std::string      str(sv);
        EXPECT_EQ(core::resources::computeStableKey64(sv),
                  core::resources::computeStableKey64(str));
    }

    // ============================================================================================
    // StringPool
    // ============================================================================================

    TEST(StringPoolTest, Intern_SameString_ReturnsSamePointer) {
        core::resources::registry::StringPool pool;

        const auto a = pool.intern("alpha");
        const auto b = pool.intern("alpha");

        // Одинаковый контент → одинаковый data() (одна аллокация в пуле).
        EXPECT_EQ(a, b);
        EXPECT_EQ(a.data(), b.data());
    }

    TEST(StringPoolTest, Intern_ChunkGrowth_OldViewRemainsValid) {
        core::resources::registry::StringPool pool;

        // Заполняем почти весь первый чанк, чтобы следующий intern гарантированно
        // выделил новый чанк. Проверяем, что старый string_view остаётся валидным.
        constexpr std::size_t kSlack = 64u;
        const std::string payload(
            core::resources::registry::StringPool::DefaultChunkSize - kSlack, 'x');

        const auto        first    = pool.intern(payload);
        const std::string snapshot(first.data(), first.size());

        constexpr std::string_view kForceNewChunk =
            "force_new_chunk_because_previous_has_too_little_space";
        const auto second = pool.intern(kForceNewChunk);

        // Старый view не должен инвалидироваться при росте пула.
        EXPECT_EQ(first, snapshot);
        EXPECT_NE(second.data(), nullptr);
    }

    TEST(StringPoolTest, ClearLookup_AllowsReinternWithDifferentStorage) {
        core::resources::registry::StringPool pool;

        const auto first = pool.intern("dup");
        pool.clearLookup();
        const auto second = pool.intern("dup");

        // Контент совпадает, но хранится в отдельной аллокации после clearLookup:
        // lookup-таблица очищена, поэтому дедупликации не происходит.
        EXPECT_EQ(first, second);
        EXPECT_NE(first.data(), second.data());
    }

} // namespace