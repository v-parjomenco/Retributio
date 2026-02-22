// ================================================================================================
// File: tests/engine/src/resource_registry_test.cpp
// Purpose: Google Tests for ResourceRegistry v1 — contract coverage.
//
// Contracts verified:
//   [RR-LOAD]           Valid JSON → correct counts + all lookup methods return expected results.
//   [RR-STABLE-ORDER]   Entries sorted ascending by StableKey64; RuntimeKey32 == sorted index.
//   [RR-DETERMINISM]    RuntimeKey32 is stable across N loads and independent of JSON key order.
//   [RR-OVERRIDE-LAYER] Higher layerPriority source wins; lower is silently dropped.
//   [RR-OVERRIDE-ORDER] Equal layerPriority: higher loadOrder wins.
//   [RR-OVERRIDE-TIE]   Tied layerPriority AND loadOrder across sources → LOG_PANIC.
//   [RR-INVALID-KEY]    Non-canonical key format → LOG_PANIC.
//   [RR-DUPLICATE-KEY-IN-SOURCE]  Same key in ≥2 blocks of one source file → LOG_PANIC.
//   [RR-STABLEKEY-COLLISION]      Hash collision across any two canonical keys → LOG_PANIC.
//   [RR-MISSING-FALLBACK-TEXTURE] No core.texture.missing in loaded set → LOG_PANIC.
//   [RR-MISSING-FALLBACK-FONT]    No core.font.default in loaded set → LOG_PANIC.
//
// Panic interception:
//   log_panic_sink_throw.cpp (linked into sfml1_engine_tests) overrides panic_sink to throw
//   std::runtime_error. expectPanic<F>(fn) catches it and returns the what() string.
//   Tests assert on structured [RR-*] markers — not on human-readable message text.
//
// StableKey collision seam:
//   stable_key_impl_seam.cpp provides an injectable hash override for this target.
//   ScopedHashOverride resets the override unconditionally on scope exit (including throws).
// ================================================================================================

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "core/resources/keys/stable_key.h"
#include "core/resources/registry/resource_registry.h"
#include "tests/engine/include/stable_key_seam.h"

namespace {

    namespace fs = std::filesystem;

    // ============================================================================================
    // TempDir
    //
    // Создаёт уникальную временную директорию на каждый экземпляр и удаляет её в деструкторе.
    //
    // Уникальность: тик steady_clock XOR с per-process атомарным счётчиком.
    //   - Два экземпляра в одном процессе гарантированно различны (счётчик).
    //   - Два процесса, стартующих одновременно, практически всегда различны (тик).
    // Нет глобальной "базовой" директории, которая сносится в конструкторе —
    // параллельные шарды ctest -j безопасны.
    // ============================================================================================
    class TempDir final {
      public:
        TempDir() {
            static std::atomic<std::uint64_t> s_counter{0u};

            const auto tick = static_cast<std::uint64_t>(
                std::chrono::steady_clock::now().time_since_epoch().count());
            const auto id = s_counter.fetch_add(1u, std::memory_order_relaxed);

            const std::string name =
                "sfml1_rr_" + std::to_string(tick ^ (id * 0x9e3779b97f4a7c15ull));

            mPath = fs::temp_directory_path() / name;

            std::error_code ec;
            fs::create_directories(mPath, ec);
            if (ec) {
                throw std::runtime_error(
                    "TempDir: не удалось создать '" + mPath.string() +
                    "': " + ec.message());
            }
        }

        ~TempDir() noexcept {
            std::error_code ec;
            fs::remove_all(mPath, ec);
            // Ошибка при удалении — косметическая в CI, деструктор не должен бросать.
        }

        TempDir(const TempDir&)            = delete;
        TempDir& operator=(const TempDir&) = delete;

        [[nodiscard]] const fs::path& path() const noexcept { return mPath; }

      private:
        fs::path mPath;
    };

    // ============================================================================================
    // Вспомогательные функции для работы с файлами
    // ============================================================================================

    void writeTextFile(const fs::path& p, std::string_view text) {
        std::ofstream out(p, std::ios::binary);
        if (!out.is_open()) {
            throw std::runtime_error(
                "writeTextFile: не удалось открыть '" + p.string() + "'");
        }
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
    }

    /// Создаёт пустой файл-заглушку: FileLoader::fileExists() вернёт true.
    void touchFile(const fs::path& p) {
        std::ofstream out(p, std::ios::binary);
        if (!out.is_open()) {
            throw std::runtime_error(
                "touchFile: не удалось создать '" + p.string() + "'");
        }
    }

    // ============================================================================================
    // Построитель JSON реестра
    // ============================================================================================

    [[nodiscard]] std::string makeRegistryJson(std::string_view textures,
                                               std::string_view fonts,
                                               std::string_view sounds) {
        std::string s;
        s.reserve(textures.size() + fonts.size() + sounds.size() + 64u);
        s += R"({"version":1,"textures":)";
        s += textures;
        s += R"(,"fonts":)";
        s += fonts;
        s += R"(,"sounds":)";
        s += sounds;
        s += '}';
        return s;
    }

    // ============================================================================================
    // Фабрика ResourceSource
    // ============================================================================================

    [[nodiscard]] core::resources::ResourceSource makeSource(const fs::path& jsonPath,
                                                             int layerPriority,
                                                             int loadOrder,
                                                             std::string_view sourceName) {
        core::resources::ResourceSource src{};
        src.path          = jsonPath.generic_string();
        src.layerPriority = layerPriority;
        src.loadOrder     = loadOrder;
        src.sourceName    = std::string(sourceName);
        return src;
    }

    // ============================================================================================
    // Хелпер проверки паники
    //
    // Шаблонный — без std::function, без type-erasure, без heap-аллокации.
    // Возвращает what() пойманного std::runtime_error для проверки маркеров [RR-*].
    // ============================================================================================

    template <class F>
    [[nodiscard]] std::string expectPanic(F&& fn) {
        try {
            std::forward<F>(fn)();
        } catch (const std::runtime_error& e) {
            return e.what();
        }
        ADD_FAILURE()
            << "Ожидалась паника (std::runtime_error) — но ничего не было брошено.";
        return {};
    }

    // ============================================================================================
    // Хелперы минимальной валидной базы
    //
    // Каждый тест, загружающий реестр, должен содержать core.texture.missing и
    // core.font.default. Эти хелперы строят канонические JSON-записи fallback-ресурсов,
    // чтобы каждый тест ломал ровно один инвариант, а не два одновременно.
    // ============================================================================================

    [[nodiscard]] std::string makeMissingTextureJson(const fs::path& filePath) {
        return "{\"core.texture.missing\":{\"path\":\"" +
               filePath.generic_string() +
               "\",\"smooth\":false,\"repeated\":false,\"mipmap\":false}}";
    }

    [[nodiscard]] std::string makeDefaultFontJson(const fs::path& filePath) {
        return "{\"core.font.default\":{\"path\":\"" +
               filePath.generic_string() + "\"}}";
    }

} // namespace

// ================================================================================================
// [RR-LOAD] Smoke: корректный JSON → счётчики и все lookup-методы работают.
// ================================================================================================
TEST(ResourceRegistryTest, LoadValidJson_CountsAndLookupsCorrect) {
    TempDir dir;

    const fs::path texMissing  = dir.path() / "missing.png";
    const fs::path fontDefault = dir.path() / "default.ttf";
    const fs::path soundClick  = dir.path() / "click.wav";

    touchFile(texMissing);
    touchFile(fontDefault);
    touchFile(soundClick);

    const std::string textures =
        "{\"core.texture.missing\":{\"path\":\"" + texMissing.generic_string() +
        "\",\"smooth\":false,\"repeated\":false,\"mipmap\":false}}";
    const std::string fonts  = makeDefaultFontJson(fontDefault);
    const std::string sounds =
        "{\"core.sound.click\":{\"path\":\"" + soundClick.generic_string() + "\"}}";

    const fs::path reg = dir.path() / "resources.json";
    writeTextFile(reg, makeRegistryJson(textures, fonts, sounds));

    core::resources::ResourceRegistry registry;
    registry.loadFromSources(std::array{makeSource(reg, 0, 0, "core")});

    EXPECT_EQ(registry.textureCount(), 1u);
    EXPECT_EQ(registry.fontCount(),    1u);
    EXPECT_EQ(registry.soundCount(),   1u);

    // findTextureByName
    const auto texKey = registry.findTextureByName("core.texture.missing");
    ASSERT_TRUE(texKey.valid());
    EXPECT_EQ(registry.getTexture(texKey).name, "core.texture.missing");

    // findTextureByStableKey
    const auto stableKey    = core::resources::computeStableKey64("core.texture.missing");
    const auto stableLookup = registry.findTextureByStableKey(stableKey);
    ASSERT_TRUE(stableLookup.valid());
    EXPECT_EQ(stableLookup, texKey);

    // findFontByName
    const auto fontKey = registry.findFontByName("core.font.default");
    ASSERT_TRUE(fontKey.valid());
    EXPECT_EQ(registry.getFont(fontKey).name, "core.font.default");

    // findSoundByName + tryGetSound
    const auto soundKey   = registry.findSoundByName("core.sound.click");
    ASSERT_TRUE(soundKey.valid());
    const auto* soundEntry = registry.tryGetSound(soundKey);
    ASSERT_NE(soundEntry, nullptr);
    EXPECT_EQ(soundEntry->name, "core.sound.click");

    // missingTextureKey / missingFontKey
    EXPECT_TRUE(registry.missingTextureKey().valid());
    EXPECT_TRUE(registry.missingFontKey().valid());
    EXPECT_EQ(registry.missingTextureKey(), texKey);
    EXPECT_EQ(registry.missingFontKey(),    fontKey);
}

// ================================================================================================
// [RR-STABLE-ORDER] Записи отсортированы по StableKey64 по возрастанию;
// RuntimeKey32 == позиция в этом отсортированном массиве.
// ================================================================================================
TEST(ResourceRegistryTest, DeterministicOrdering_ByStableKey64) {
    TempDir dir;

    const fs::path texMissing  = dir.path() / "missing.png";
    const fs::path texAlpha    = dir.path() / "alpha.png";
    const fs::path fontDefault = dir.path() / "default.ttf";

    touchFile(texMissing);
    touchFile(texAlpha);
    touchFile(fontDefault);

    const std::string textures =
        "{\"core.texture.missing\":{\"path\":\"" + texMissing.generic_string() +
        "\"},\"test.texture.alpha\":{\"path\":\"" + texAlpha.generic_string() + "\"}}";

    const fs::path reg = dir.path() / "registry.json";
    writeTextFile(reg, makeRegistryJson(textures, makeDefaultFontJson(fontDefault), "{}"));

    core::resources::ResourceRegistry registry;
    registry.loadFromSources(std::array{makeSource(reg, 0, 0, "src0")});

    std::vector<std::uint64_t> actual;
    actual.reserve(registry.textureCount());

    for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(registry.textureCount()); ++i) {
        const auto  key   = core::resources::TextureKey::make(i);
        const auto& entry = registry.getTexture(key);
        actual.push_back(entry.stableKey);
        EXPECT_EQ(entry.key.index(), i);
        EXPECT_EQ(entry.stableKey, core::resources::computeStableKey64(entry.name));
    }

    std::vector<std::uint64_t> expected;
    expected.push_back(core::resources::computeStableKey64("core.texture.missing"));
    expected.push_back(core::resources::computeStableKey64("test.texture.alpha"));
    std::sort(expected.begin(), expected.end());

    EXPECT_EQ(actual, expected);
}

// ================================================================================================
// [RR-DETERMINISM] RuntimeKey32 стабилен при 100 перезагрузках одного набора источников.
// ================================================================================================
TEST(ResourceRegistryTest, Determinism_IndicesStableAcross100Loads) {
    TempDir dir;

    const fs::path texMissing   = dir.path() / "missing.png";
    const fs::path texSecondary = dir.path() / "secondary.png";
    const fs::path fontDefault  = dir.path() / "default.ttf";

    touchFile(texMissing);
    touchFile(texSecondary);
    touchFile(fontDefault);

    const std::string textures =
        "{\"core.texture.missing\":{\"path\":\"" + texMissing.generic_string() +
        "\"},\"core.texture.secondary\":{\"path\":\"" + texSecondary.generic_string() +
        "\"}}";

    const fs::path reg = dir.path() / "registry.json";
    writeTextFile(reg, makeRegistryJson(textures, makeDefaultFontJson(fontDefault), "{}"));

    const auto sources = std::array{makeSource(reg, 0, 0, "core")};

    std::uint32_t baselineMissing   = 0u;
    std::uint32_t baselineSecondary = 0u;

    for (int i = 0; i < 100; ++i) {
        core::resources::ResourceRegistry registry;
        registry.loadFromSources(sources);

        const auto missingKey   = registry.findTextureByName("core.texture.missing");
        const auto secondaryKey = registry.findTextureByName("core.texture.secondary");

        ASSERT_TRUE(missingKey.valid());
        ASSERT_TRUE(secondaryKey.valid());

        if (i == 0) {
            baselineMissing   = missingKey.index();
            baselineSecondary = secondaryKey.index();
        } else {
            EXPECT_EQ(missingKey.index(),   baselineMissing)
                << "RuntimeKey32 изменился на итерации " << i;
            EXPECT_EQ(secondaryKey.index(), baselineSecondary)
                << "RuntimeKey32 изменился на итерации " << i;
        }
    }
}

// ================================================================================================
// [RR-DETERMINISM] RuntimeKey32 не зависит от порядка ключей внутри JSON.
// ================================================================================================
TEST(ResourceRegistryTest, Determinism_IndependentOfJsonKeyOrder) {
    TempDir dir;

    const fs::path texMissing   = dir.path() / "missing.png";
    const fs::path texSecondary = dir.path() / "secondary.png";
    const fs::path fontDefault  = dir.path() / "default.ttf";

    touchFile(texMissing);
    touchFile(texSecondary);
    touchFile(fontDefault);

    const std::string fonts = makeDefaultFontJson(fontDefault);

    // Порядок A: missing → secondary
    const std::string texOrderA =
        "{\"core.texture.missing\":{\"path\":\"" + texMissing.generic_string() +
        "\"},\"core.texture.secondary\":{\"path\":\"" + texSecondary.generic_string() +
        "\"}}";

    // Порядок B: secondary → missing (ключи переставлены)
    const std::string texOrderB =
        "{\"core.texture.secondary\":{\"path\":\"" + texSecondary.generic_string() +
        "\"},\"core.texture.missing\":{\"path\":\"" + texMissing.generic_string() +
        "\"}}";

    const fs::path regA = dir.path() / "order_a.json";
    const fs::path regB = dir.path() / "order_b.json";
    writeTextFile(regA, makeRegistryJson(texOrderA, fonts, "{}"));
    writeTextFile(regB, makeRegistryJson(texOrderB, fonts, "{}"));

    core::resources::ResourceRegistry regA_r;
    regA_r.loadFromSources(std::array{makeSource(regA, 0, 0, "a")});

    core::resources::ResourceRegistry regB_r;
    regB_r.loadFromSources(std::array{makeSource(regB, 0, 0, "b")});

    const auto mA = regA_r.findTextureByName("core.texture.missing");
    const auto sA = regA_r.findTextureByName("core.texture.secondary");
    const auto mB = regB_r.findTextureByName("core.texture.missing");
    const auto sB = regB_r.findTextureByName("core.texture.secondary");

    ASSERT_TRUE(mA.valid()); ASSERT_TRUE(sA.valid());
    ASSERT_TRUE(mB.valid()); ASSERT_TRUE(sB.valid());

    EXPECT_EQ(mA.index(), mB.index())
        << "RuntimeKey32 для 'missing' не должен зависеть от порядка ключей в JSON";
    EXPECT_EQ(sA.index(), sB.index())
        << "RuntimeKey32 для 'secondary' не должен зависеть от порядка ключей в JSON";
}

// ================================================================================================
// [RR-OVERRIDE-LAYER] Источник с более высоким layerPriority побеждает.
//
// Регрессионный тест: отсутствовал в предыдущей версии тест-сьюта.
// Сценарий:
//   - core (layerPriority=0): определяет test.texture.override → core-путь
//   - mod  (layerPriority=10): определяет test.texture.override → mod-путь
//   Ожидается: entry.path == mod-путь; textureCount остаётся равным 2 (override, не add).
// ================================================================================================
TEST(ResourceRegistryTest, OverridePolicy_HigherLayerPriority_Wins) {
    TempDir dir;

    const fs::path texMissing  = dir.path() / "missing.png";
    const fs::path fontDefault = dir.path() / "default.ttf";
    const fs::path texCore     = dir.path() / "override_core.png";
    const fs::path texMod      = dir.path() / "override_mod.png";

    touchFile(texMissing);
    touchFile(fontDefault);
    touchFile(texCore);
    touchFile(texMod);

    const std::string fonts = makeDefaultFontJson(fontDefault);

    const std::string coreTextures =
        "{\"core.texture.missing\":{\"path\":\"" + texMissing.generic_string() +
        "\"},\"test.texture.override\":{\"path\":\"" + texCore.generic_string() + "\"}}";

    // mod-источник содержит только оспариваемый ключ.
    const std::string modTextures =
        "{\"test.texture.override\":{\"path\":\"" + texMod.generic_string() + "\"}}";

    const fs::path coreReg = dir.path() / "core.json";
    const fs::path modReg  = dir.path() / "mod.json";
    writeTextFile(coreReg, makeRegistryJson(coreTextures, fonts, "{}"));
    writeTextFile(modReg,  makeRegistryJson(modTextures,  fonts, "{}"));

    core::resources::ResourceRegistry registry;
    registry.loadFromSources(std::array{
        makeSource(coreReg,  0, 0, "core"),
        makeSource(modReg,  10, 0, "mod")
    });

    const auto key = registry.findTextureByName("test.texture.override");
    ASSERT_TRUE(key.valid());

    EXPECT_EQ(registry.getTexture(key).path, texMod.generic_string())
        << "Источник с более высоким layerPriority должен переопределять низший";

    // Override заменяет, а не добавляет — счётчик остаётся 2.
    EXPECT_EQ(registry.textureCount(), 2u);
}

// ================================================================================================
// [RR-OVERRIDE-LAYER] Источник с более низким layerPriority не переопределяет высший,
// даже если загружается после него.
// ================================================================================================
TEST(ResourceRegistryTest, OverridePolicy_LowerLayerPriority_IsDropped) {
    TempDir dir;

    const fs::path texMissing  = dir.path() / "missing.png";
    const fs::path fontDefault = dir.path() / "default.ttf";
    const fs::path texHigh     = dir.path() / "high.png";
    const fs::path texLow      = dir.path() / "low.png";

    touchFile(texMissing);
    touchFile(fontDefault);
    touchFile(texHigh);
    touchFile(texLow);

    const std::string fonts = makeDefaultFontJson(fontDefault);

    const std::string highTextures =
        "{\"core.texture.missing\":{\"path\":\"" + texMissing.generic_string() +
        "\"},\"test.texture.contested\":{\"path\":\"" + texHigh.generic_string() + "\"}}";

    const std::string lowTextures =
        "{\"test.texture.contested\":{\"path\":\"" + texLow.generic_string() + "\"}}";

    const fs::path highReg = dir.path() / "high.json";
    const fs::path lowReg  = dir.path() / "low.json";
    writeTextFile(highReg, makeRegistryJson(highTextures, fonts, "{}"));
    writeTextFile(lowReg,  makeRegistryJson(lowTextures,  fonts, "{}"));

    // high загружается первым, low — вторым; low всё равно должен проиграть.
    core::resources::ResourceRegistry registry;
    registry.loadFromSources(std::array{
        makeSource(highReg, 10, 0, "high"),
        makeSource(lowReg,   0, 0, "low")
    });

    const auto key = registry.findTextureByName("test.texture.contested");
    ASSERT_TRUE(key.valid());

    EXPECT_EQ(registry.getTexture(key).path, texHigh.generic_string())
        << "Источник с меньшим layerPriority не должен переопределять высший";
}

// ================================================================================================
// [RR-OVERRIDE-ORDER] При равном layerPriority побеждает источник с большим loadOrder.
// ================================================================================================
TEST(ResourceRegistryTest, OverridePolicy_EqualLayer_HigherLoadOrder_Wins) {
    TempDir dir;

    const fs::path texMissing  = dir.path() / "missing.png";
    const fs::path fontDefault = dir.path() / "default.ttf";
    const fs::path texEarly    = dir.path() / "early.png";
    const fs::path texLate     = dir.path() / "late.png";

    touchFile(texMissing);
    touchFile(fontDefault);
    touchFile(texEarly);
    touchFile(texLate);

    const std::string fonts = makeDefaultFontJson(fontDefault);

    const std::string earlyTextures =
        "{\"core.texture.missing\":{\"path\":\"" + texMissing.generic_string() +
        "\"},\"test.texture.order\":{\"path\":\"" + texEarly.generic_string() + "\"}}";

    const std::string lateTextures =
        "{\"test.texture.order\":{\"path\":\"" + texLate.generic_string() + "\"}}";

    const fs::path earlyReg = dir.path() / "early.json";
    const fs::path lateReg  = dir.path() / "late.json";
    writeTextFile(earlyReg, makeRegistryJson(earlyTextures, fonts, "{}"));
    writeTextFile(lateReg,  makeRegistryJson(lateTextures,  fonts, "{}"));

    core::resources::ResourceRegistry registry;
    registry.loadFromSources(std::array{
        makeSource(earlyReg, 0, 1, "early"),  // layerPriority=0, loadOrder=1
        makeSource(lateReg,  0, 5, "late")    // layerPriority=0, loadOrder=5  ← должен победить
    });

    const auto key = registry.findTextureByName("test.texture.order");
    ASSERT_TRUE(key.valid());

    EXPECT_EQ(registry.getTexture(key).path, texLate.generic_string())
        << "Больший loadOrder должен побеждать при равном layerPriority";
}

// ================================================================================================
// [RR-OVERRIDE-TIE] Одинаковые layerPriority и loadOrder в двух источниках → LOG_PANIC.
//
// Минимально валидная база: оба источника включают core.texture.missing и core.font.default,
// чтобы паника сработала именно в точке обнаружения ничьи, а не на отсутствии fallback.
// ================================================================================================
TEST(ResourceRegistryTest, OverridePolicy_TiedLayerAndOrder_IsPanic) {
    TempDir dir;

    const fs::path texMissing  = dir.path() / "missing.png";
    const fs::path fontDefault = dir.path() / "default.ttf";
    const fs::path texSrc1     = dir.path() / "same_1.png";
    const fs::path texSrc2     = dir.path() / "same_2.png";

    touchFile(texMissing);
    touchFile(fontDefault);
    touchFile(texSrc1);
    touchFile(texSrc2);

    const std::string fonts = makeDefaultFontJson(fontDefault);

    const fs::path reg1 = dir.path() / "source_1.json";
    const fs::path reg2 = dir.path() / "source_2.json";

    writeTextFile(reg1, makeRegistryJson(
        "{\"core.texture.missing\":{\"path\":\"" + texMissing.generic_string() +
        "\"},\"test.texture.same\":{\"path\":\"" + texSrc1.generic_string() + "\"}}",
        fonts, "{}"));

    writeTextFile(reg2, makeRegistryJson(
        "{\"test.texture.same\":{\"path\":\"" + texSrc2.generic_string() + "\"}}",
        fonts, "{}"));

    const std::string msg = expectPanic([&]() {
        core::resources::ResourceRegistry registry;
        registry.loadFromSources(std::array{
            makeSource(reg1, 10, 10, "A"),
            makeSource(reg2, 10, 10, "B")});
    });

    EXPECT_NE(msg.find("[RR-OVERRIDE-TIE]"), std::string::npos)
        << "Паника должна содержать маркер [RR-OVERRIDE-TIE]. Получено: " << msg;
}

// ================================================================================================
// [RR-INVALID-KEY] Ключ с недопустимым форматом (заглавная буква) → LOG_PANIC.
// ================================================================================================
TEST(ResourceRegistryTest, CanonicalKey_Invalid_IsPanic) {
    TempDir dir;

    const fs::path texMissing  = dir.path() / "missing.png";
    const fs::path fontDefault = dir.path() / "default.ttf";
    const fs::path badTex      = dir.path() / "bad.png";

    touchFile(texMissing);
    touchFile(fontDefault);
    touchFile(badTex);

    const fs::path reg = dir.path() / "bad_key.json";
    writeTextFile(reg, makeRegistryJson(
        "{\"core.texture.missing\":{\"path\":\"" + texMissing.generic_string() +
        "\"},\"Bad.texture.key\":{\"path\":\"" + badTex.generic_string() + "\"}}",
        makeDefaultFontJson(fontDefault),
        "{}"));

    const std::string msg = expectPanic([&]() {
        core::resources::ResourceRegistry registry;
        registry.loadFromSources(std::array{makeSource(reg, 0, 0, "src0")});
    });

    EXPECT_NE(msg.find("[RR-INVALID-KEY]"), std::string::npos)
        << "Паника должна содержать маркер [RR-INVALID-KEY]. Получено: " << msg;
}

// ================================================================================================
// [RR-DUPLICATE-KEY-IN-SOURCE] Один canonical key в двух блоках одного источника → LOG_PANIC.
// SourceKeySet глобален по файлу, не по блоку.
// ================================================================================================
TEST(ResourceRegistryTest, DuplicateCanonicalKeyAcrossBlocks_InSingleSource_IsPanic) {
    TempDir dir;

    const fs::path texMissing  = dir.path() / "missing.png";
    const fs::path fontDefault = dir.path() / "default.ttf";
    const fs::path texDup      = dir.path() / "dup.png";
    const fs::path fontDup     = dir.path() / "dup.ttf";

    touchFile(texMissing);
    touchFile(fontDefault);
    touchFile(texDup);
    touchFile(fontDup);

    const fs::path reg = dir.path() / "dup.json";
    writeTextFile(reg, makeRegistryJson(
        "{\"core.texture.missing\":{\"path\":\"" + texMissing.generic_string() +
        "\"},\"dup.shared.key\":{\"path\":\"" + texDup.generic_string() + "\"}}",
        "{\"core.font.default\":{\"path\":\"" + fontDefault.generic_string() +
        "\"},\"dup.shared.key\":{\"path\":\"" + fontDup.generic_string() + "\"}}",
        "{}"));

    const std::string msg = expectPanic([&]() {
        core::resources::ResourceRegistry registry;
        registry.loadFromSources(std::array{makeSource(reg, 0, 0, "src0")});
    });

    EXPECT_NE(msg.find("[RR-DUPLICATE-KEY-IN-SOURCE]"), std::string::npos)
        << "Паника должна содержать маркер [RR-DUPLICATE-KEY-IN-SOURCE]. Получено: " << msg;
}

// ================================================================================================
// [RR-STABLEKEY-COLLISION] Два canonical key с одинаковым StableKey64 → LOG_PANIC.
//
// Реальные коллизии xxHash3_64 на 64-бит практически недостижимы в тесте.
// Используем link-time seam (stable_key_impl_seam.cpp): инжектируем константную хэш-функцию.
// ScopedHashOverride сбрасывает override при выходе из scope, включая выход по исключению.
// ================================================================================================
TEST(ResourceRegistryTest, StableKey_Collision_IsPanic) {
    // Каждый ключ хэшируется в одно и то же значение → гарантированная коллизия.
	constexpr auto kConstantHash = std::uint64_t{0xDEADBEEFCAFEBABEull};

	const core::resources::detail::ScopedHashOverride guard{
		[](std::string_view) noexcept -> std::uint64_t { return kConstantHash; }};

    TempDir dir;

    const fs::path texMissing  = dir.path() / "missing.png";
    const fs::path texOther    = dir.path() / "other.png";
    const fs::path fontDefault = dir.path() / "default.ttf";

    touchFile(texMissing);
    touchFile(texOther);
    touchFile(fontDefault);

    // Два различных texture key → оба получают одинаковый хэш → коллизия.
    const std::string textures =
        "{\"core.texture.missing\":{\"path\":\"" + texMissing.generic_string() +
        "\"},\"test.texture.other\":{\"path\":\"" + texOther.generic_string() + "\"}}";

    const fs::path reg = dir.path() / "collision.json";
    writeTextFile(reg, makeRegistryJson(textures, makeDefaultFontJson(fontDefault), "{}"));

    const std::string msg = expectPanic([&]() {
        core::resources::ResourceRegistry registry;
        registry.loadFromSources(std::array{makeSource(reg, 0, 0, "core")});
    });

    EXPECT_NE(msg.find("[RR-STABLEKEY-COLLISION]"), std::string::npos)
        << "Паника должна содержать маркер [RR-STABLEKEY-COLLISION]. Получено: " << msg;
}

// ================================================================================================
// [RR-MISSING-FALLBACK-TEXTURE] Реестр без core.texture.missing → LOG_PANIC.
// ================================================================================================
TEST(ResourceRegistryTest, MissingFallbackTexture_IsPanic) {
    TempDir dir;

    const fs::path fontDefault = dir.path() / "default.ttf";
    const fs::path texOther    = dir.path() / "other.png";
    touchFile(fontDefault);
    touchFile(texOther);

    // Намеренно не добавляем core.texture.missing.
    const fs::path reg = dir.path() / "no_missing_tex.json";
    writeTextFile(reg, makeRegistryJson(
        "{\"test.texture.other\":{\"path\":\"" + texOther.generic_string() + "\"}}",
        makeDefaultFontJson(fontDefault),
        "{}"));

    const std::string msg = expectPanic([&]() {
        core::resources::ResourceRegistry registry;
        registry.loadFromSources(std::array{makeSource(reg, 0, 0, "core")});
    });

    EXPECT_NE(msg.find("[RR-MISSING-FALLBACK-TEXTURE]"), std::string::npos)
        << "Паника должна содержать маркер [RR-MISSING-FALLBACK-TEXTURE]. Получено: " << msg;
}

// ================================================================================================
// [RR-MISSING-FALLBACK-FONT] Реестр без core.font.default → LOG_PANIC.
// ================================================================================================
TEST(ResourceRegistryTest, MissingFallbackFont_IsPanic) {
    TempDir dir;

    const fs::path texMissing = dir.path() / "missing.png";
    const fs::path fontOther  = dir.path() / "other.ttf";
    touchFile(texMissing);
    touchFile(fontOther);

    // Намеренно не добавляем core.font.default.
    const fs::path reg = dir.path() / "no_missing_font.json";
    writeTextFile(reg, makeRegistryJson(
        makeMissingTextureJson(texMissing),
        "{\"test.font.other\":{\"path\":\"" + fontOther.generic_string() + "\"}}",
        "{}"));

    const std::string msg = expectPanic([&]() {
        core::resources::ResourceRegistry registry;
        registry.loadFromSources(std::array{makeSource(reg, 0, 0, "core")});
    });

    EXPECT_NE(msg.find("[RR-MISSING-FALLBACK-FONT]"), std::string::npos)
        << "Паника должна содержать маркер [RR-MISSING-FALLBACK-FONT]. Получено: " << msg;
}