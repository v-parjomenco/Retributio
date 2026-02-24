// ================================================================================================
// File: tests/engine/src/resource_manager_test.cpp
// Purpose: Google Tests for ResourceManager key-based API (init, lazy-load, resident-only, preload,
//          metrics, cache generation, clearAll, IO-forbidden contract).
// Used by: sfml1_engine_tests
// Related headers: core/resources/resource_manager.h
// Notes:
//  - I/O is injected via ResourceManager::Loaders (no SFML1_TESTS hooks, no global mutable state).
//  - Each test uses a unique temp directory instance (safe for parallel execution).
//  - Placeholder files are non-empty to avoid coupling to "empty file" semantics.
//  - Panic assertions use expectPanic<F>() exclusively. ASSERT_DEATH / EXPECT_DEATH are
//    forbidden in this binary: sfml1_engine_tests links log_panic_sink_throw.cpp which
//    replaces panic_sink with a throw — incompatible with GTest death-test subprocess model.
// ================================================================================================
#include "core/resources/resource_manager.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "test_panic.h"

namespace {

    namespace fs = std::filesystem;

    using test_support::expectPanic;

    // --------------------------------------------------------------------------------------------
    // Мелкие утилиты
    // --------------------------------------------------------------------------------------------
    [[nodiscard]] std::string toHex16(std::uint64_t v) {
        static constexpr char kHex[] = "0123456789abcdef";

        std::string s(16, '0');
        for (int i = 15; i >= 0; --i) {
            s[static_cast<std::size_t>(i)] = kHex[v & 0x0Fu];
            v >>= 4u;
        }
        return s;
    }

    [[nodiscard]] std::uint64_t makeUniqueSeed() {
        static std::atomic_uint64_t counter{0u};

        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
        const std::uint64_t c = counter.fetch_add(1u, std::memory_order_relaxed) + 1u;

        return static_cast<std::uint64_t>(now) ^ (static_cast<std::uint64_t>(tid) << 1u) ^ c;
    }

    void writeBytesOrThrow(const fs::path& path, std::string_view bytes) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            throw std::runtime_error("writeBytesOrThrow: failed to open file");
        }

        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        out.flush();

        if (!out.good()) {
            throw std::runtime_error("writeBytesOrThrow: failed to write/flush");
        }
    }

    // --------------------------------------------------------------------------------------------
    // TempDir — уникальная директория на тест, безопасно для параллельных раннеров.
    // Удаляет только свой инстанс, не трогает базовую папку.
    // --------------------------------------------------------------------------------------------
    class TempDir final {
    public:
        explicit TempDir(std::string_view tag) {
            const fs::path base = fs::temp_directory_path() / "sfml1_tests_resource_manager";

            std::error_code ec;
            fs::create_directories(base, ec);
            if (ec) {
                throw std::runtime_error("TempDir: failed to create base directory");
            }

            for (int attempt = 0; attempt < 8; ++attempt) {
                const std::string name =
                    std::string(tag) + "_" + toHex16(makeUniqueSeed());

                mPath = base / name;

                ec.clear();
                const bool created = fs::create_directory(mPath, ec);
                if (created) {
                    return;
                }

                if (ec) {
                    throw std::runtime_error("TempDir: failed to create unique directory");
                }
            }

            throw std::runtime_error("TempDir: exhausted attempts to create unique directory");
        }

        ~TempDir() {
            std::error_code ec;
            if (!mPath.empty()) {
                fs::remove_all(mPath, ec);
            }
        }

        TempDir(const TempDir&) = delete;
        TempDir& operator=(const TempDir&) = delete;

        TempDir(TempDir&& other) noexcept
            : mPath(std::move(other.mPath)) {
            other.mPath.clear();
        }

        TempDir& operator=(TempDir&& other) noexcept {
            if (this == &other) {
                return *this;
            }

            std::error_code ec;
            if (!mPath.empty()) {
                fs::remove_all(mPath, ec);
            }

            mPath = std::move(other.mPath);
            other.mPath.clear();
            return *this;
        }

        [[nodiscard]] const fs::path& path() const noexcept { return mPath; }

    private:
        fs::path mPath;
    };

    [[nodiscard]] std::string makeRegistryJson(std::string_view textures,
        std::string_view fonts,
        std::string_view sounds) {
        std::string s;
        s.reserve(textures.size() + fonts.size() + sounds.size() + 64u);

        s += "{\"version\":1,\"textures\":";
        s += textures;
        s += ",\"fonts\":";
        s += fonts;
        s += ",\"sounds\":";
        s += sounds;
        s += "}";

        return s;
    }

    void appendPathEntry(std::string& out, std::string_view key, const fs::path& path) {
        out += '"';
        out += key;
        out += "\":{\"path\":\"";
        out += path.generic_string();
        out += "\"}";
    }

    core::resources::ResourceSource makeSource(const fs::path& jsonPath,
        int layerPriority,
        int loadOrder,
        std::string_view sourceName) {
        core::resources::ResourceSource s{};
        s.path = jsonPath.generic_string();
        s.layerPriority = layerPriority;
        s.loadOrder = loadOrder;
        s.sourceName = std::string(sourceName);
        return s;
    }

    [[nodiscard]] std::string currentTestTagOrFallback() {
        const auto* ut = ::testing::UnitTest::GetInstance();
        const auto* ti = (ut != nullptr) ? ut->current_test_info() : nullptr;
        if (ti == nullptr) {
            return "resource_manager";
        }

        std::string tag;
        tag.reserve(96u);
        tag += ti->test_suite_name();
        tag += "_";
        tag += ti->name();
        return tag;
    }

} // namespace

// ================================================================================================
// Фикстура ResourceManagerTest
// ================================================================================================
class ResourceManagerTest : public ::testing::Test {
protected:
    int textureLoads = 0;
    int fontLoads = 0;
    int soundLoads = 0;
    bool failSound = false;

    struct Env {
        explicit Env(std::string tag)
            : dir(std::move(tag)) {
        }

        TempDir dir;
        fs::path jsonPath;

        fs::path missingTexturePath;
        fs::path secondaryTexturePath;

        fs::path defaultFontPath;
        fs::path secondaryFontPath;

        fs::path soundOkPath;
        fs::path soundFailPath;

        bool hasSecondaryFont = false;
    };

    class TestSoundResource final : public core::resources::ISoundResource {
      public:
        explicit TestSoundResource(ResourceManagerTest& owner)
            : mOwner(owner) {
        }

        [[nodiscard]] bool loadFromFile(std::string_view path) override {
            ++mOwner.soundLoads;
            return !(mOwner.failSound && path.ends_with("fail.wav"));
        }

      private:
        ResourceManagerTest& mOwner;
    };

    bool onLoadTexture(core::resources::types::TextureResource& /* out */,
                       std::string_view /* path */,
                       bool /* sRgb */) {
        ++textureLoads;
        return true;
    }

    bool onLoadFont(core::resources::types::FontResource& /* out */,
                    std::string_view /* path */) {
        ++fontLoads;
        return true;
    }

    std::unique_ptr<core::resources::ISoundResource> onCreateSound() {
        return std::make_unique<TestSoundResource>(*this);
    }

    [[nodiscard]] core::resources::ResourceManager::Loaders makeLoaders() {
        core::resources::ResourceManager::Loaders l{};

        l.bindTexture<ResourceManagerTest, &ResourceManagerTest::onLoadTexture>(*this);
        l.bindFont<ResourceManagerTest, &ResourceManagerTest::onLoadFont>(*this);
        l.bindSoundFactory<ResourceManagerTest, &ResourceManagerTest::onCreateSound>(*this);

        return l;
    }

    [[nodiscard]] Env makeEnv(bool withSecondaryFont) {
        Env env(currentTestTagOrFallback());
        env.hasSecondaryFont = withSecondaryFont;

        env.missingTexturePath = env.dir.path() / "missing.png";
        env.secondaryTexturePath = env.dir.path() / "secondary.png";

        env.defaultFontPath = env.dir.path() / "default.ttf";
        env.secondaryFontPath = env.dir.path() / "secondary.ttf";

        env.soundOkPath = env.dir.path() / "click.wav";
        env.soundFailPath = env.dir.path() / "fail.wav";

        // Не пустые файлы: тесты не должны зависеть от трактовки "0 bytes file".
        writeBytesOrThrow(env.missingTexturePath, "tex_missing");
        writeBytesOrThrow(env.secondaryTexturePath, "tex_secondary");

        writeBytesOrThrow(env.defaultFontPath, "font_default");
        if (withSecondaryFont) {
            writeBytesOrThrow(env.secondaryFontPath, "font_secondary");
        }

        writeBytesOrThrow(env.soundOkPath, "sound_ok");
        writeBytesOrThrow(env.soundFailPath, "sound_fail");

        std::string textures;
        textures.reserve(256u);
        textures += "{";
        appendPathEntry(textures, "core.texture.missing", env.missingTexturePath);
        textures += ",";
        appendPathEntry(textures, "core.texture.secondary", env.secondaryTexturePath);
        textures += "}";

        std::string fonts;
        fonts.reserve(256u);
        fonts += "{";
        appendPathEntry(fonts, "core.font.default", env.defaultFontPath);
        if (withSecondaryFont) {
            fonts += ",";
            appendPathEntry(fonts, "core.font.secondary", env.secondaryFontPath);
        }
        fonts += "}";

        std::string sounds;
        sounds.reserve(256u);
        sounds += "{";
        appendPathEntry(sounds, "core.sound.click", env.soundOkPath);
        sounds += ",";
        appendPathEntry(sounds, "core.sound.fail", env.soundFailPath);
        sounds += "}";

        env.jsonPath = env.dir.path() / "resources.json";
        writeBytesOrThrow(env.jsonPath, makeRegistryJson(textures, fonts, sounds));

        return env;
    }

    void initialize(core::resources::ResourceManager& manager, const Env& env) {
        manager.initialize(std::array{ makeSource(env.jsonPath, 0, 0, "core") }, makeLoaders());
    }
};

// ------------------------------------------------------------------------------------------------
// Контракт boot-фазы: counts, missing keys, eager-load missing texture/font, cache generation.
// ------------------------------------------------------------------------------------------------
TEST_F(ResourceManagerTest, Initialize_CountsMissingKeysAndCacheGeneration) {
    const auto env = makeEnv(false);

    core::resources::ResourceManager manager;
    initialize(manager, env);

    const auto& reg = manager.registry();
    EXPECT_EQ(reg.textureCount(), 2u);
    EXPECT_EQ(reg.fontCount(), 1u);
    EXPECT_EQ(reg.soundCount(), 2u);

    EXPECT_TRUE(reg.missingTextureKey().valid());
    EXPECT_TRUE(reg.missingFontKey().valid());

    EXPECT_EQ(manager.missingTextureKey(), reg.missingTextureKey());
    EXPECT_EQ(manager.missingFontKey(), reg.missingFontKey());

    // initialize() обязана сделать missing texture + missing font resident, звуки остаются lazy.
    EXPECT_EQ(textureLoads, 1);
    EXPECT_EQ(fontLoads, 1);
    EXPECT_EQ(soundLoads, 0);

    const std::uint32_t gen = manager.cacheGeneration();
    EXPECT_NE(gen, 0u);
}

// ------------------------------------------------------------------------------------------------
// getTexture(): resident key не должен догружаться; invalid/out-of-range -> fallback без load.
// ------------------------------------------------------------------------------------------------
TEST_F(ResourceManagerTest, GetTexture_LoadOnceAndFallback_NoExtraLoads) {
    const auto env = makeEnv(false);

    core::resources::ResourceManager manager;
    initialize(manager, env);

    ASSERT_EQ(textureLoads, 1);

    const auto missing = manager.missingTextureKey();
    const auto& t1 = manager.getTexture(missing);
    const auto& t2 = manager.getTexture(missing);

    EXPECT_EQ(&t1, &t2);
    EXPECT_EQ(textureLoads, 1);

    const int before = textureLoads;

    const auto& f1 = manager.getTexture(core::resources::TextureKey{});
    const auto& f2 = manager.getTexture(core::resources::TextureKey::make(999u));

    EXPECT_EQ(&t1, &f1);
    EXPECT_EQ(&t1, &f2);
    EXPECT_EQ(textureLoads, before);
}

// ------------------------------------------------------------------------------------------------
// getFont(): resident key не должен догружаться; invalid/out-of-range -> fallback без load.
// ------------------------------------------------------------------------------------------------
TEST_F(ResourceManagerTest, GetFont_LoadOnceAndFallback_NoExtraLoads) {
    const auto env = makeEnv(false);

    core::resources::ResourceManager manager;
    initialize(manager, env);

    ASSERT_EQ(fontLoads, 1);

    const auto missing = manager.missingFontKey();
    const auto& f1 = manager.getFont(missing);
    const auto& f2 = manager.getFont(missing);

    EXPECT_EQ(&f1, &f2);
    EXPECT_EQ(fontLoads, 1);

    const int before = fontLoads;

    const auto& r1 = manager.getFont(core::resources::FontKey{});
    const auto& r2 = manager.getFont(core::resources::FontKey::make(999u));

    EXPECT_EQ(&f1, &r1);
    EXPECT_EQ(&f1, &r2);
    EXPECT_EQ(fontLoads, before);
}

// ------------------------------------------------------------------------------------------------
// findTexture(): возвращает key; secondary грузится по требованию ровно один раз.
// Дополнительно: cacheGeneration НЕ должен меняться из-за lazy-load.
// Меняется только init/clearAll.
// ------------------------------------------------------------------------------------------------
TEST_F(ResourceManagerTest, FindTexture_LoadsSecondaryExactlyOnce_NoGenerationBump) {
    const auto env = makeEnv(false);

    core::resources::ResourceManager manager;
    initialize(manager, env);

    const std::uint32_t genBefore = manager.cacheGeneration();

    const int baseline = textureLoads;
    ASSERT_EQ(baseline, 1);

    const auto key = manager.findTexture("core.texture.secondary");
    ASSERT_TRUE(key.valid());

    const auto& a = manager.getTexture(key);
    const auto& b = manager.getTexture(key);

    EXPECT_EQ(&a, &b);
    EXPECT_EQ(textureLoads, baseline + 1);

    const std::uint32_t genAfter = manager.cacheGeneration();
    EXPECT_EQ(genAfter, genBefore);
}

// ------------------------------------------------------------------------------------------------
// findFont(): тот же контракт, что и для findTexture().
// ------------------------------------------------------------------------------------------------
TEST_F(ResourceManagerTest, FindFont_LoadsSecondaryExactlyOnce) {
    const auto env = makeEnv(true);

    core::resources::ResourceManager manager;
    initialize(manager, env);

    ASSERT_EQ(fontLoads, 1);

    const auto key = manager.findFont("core.font.secondary");
    ASSERT_TRUE(key.valid());

    const auto& a = manager.getFont(key);
    const auto& b = manager.getFont(key);

    EXPECT_EQ(&a, &b);
    EXPECT_EQ(fontLoads, 2);
}

// ------------------------------------------------------------------------------------------------
// Контракт звуков:
//  - invalid key -> nullptr
//  - soft-fail: одна попытка, затем Failed; retry запрещён, даже если
//    загрузчик позже бы "починился".
// ------------------------------------------------------------------------------------------------
TEST_F(ResourceManagerTest, TryGetSound_InvalidKeyNullptr_SoftFailNoRetry) {
    const auto env = makeEnv(false);

    core::resources::ResourceManager manager;
    initialize(manager, env);

    EXPECT_FALSE(manager.tryGetSound(core::resources::SoundKey{}).valid());

    const auto failKey = manager.findSound("core.sound.fail");
    ASSERT_TRUE(failKey.valid());

    soundLoads = 0;
    failSound = true;

    const auto s1 = manager.tryGetSound(failKey);
    const auto s2 = manager.tryGetSound(failKey);

    EXPECT_FALSE(s1.valid());
    EXPECT_FALSE(s2.valid());
    EXPECT_EQ(soundLoads, 1);

    // Даже если бы загрузчик теперь "починился", retry запрещён контрактом Failed-state.
    failSound = false;

    const int before = soundLoads;
    const auto s3 = manager.tryGetSound(failKey);

    EXPECT_FALSE(s3.valid());
    EXPECT_EQ(soundLoads, before);
}

// ------------------------------------------------------------------------------------------------
// Resident-only API:
//  - expect*Resident(invalid/out-of-range) мапится на missing и не делает I/O
//  - tryGetSoundResident возвращает nullptr, пока звук не resident
// ------------------------------------------------------------------------------------------------
TEST_F(ResourceManagerTest, ResidentOnlyApi_InvalidMapsToMissing_NoIo) {
    const auto env = makeEnv(false);

    core::resources::ResourceManager manager;
    initialize(manager, env);

    const int texBefore = textureLoads;
    const int fontBefore = fontLoads;
    const int soundBefore = soundLoads;

    const auto& missingTex = manager.getTexture(manager.missingTextureKey());
    const auto& missingFont = manager.getFont(manager.missingFontKey());

    const auto& t = manager.expectTextureResident(core::resources::TextureKey{});
    const auto& f = manager.expectFontResident(core::resources::FontKey{});

    EXPECT_EQ(&t, &missingTex);
    EXPECT_EQ(&f, &missingFont);

    EXPECT_EQ(textureLoads, texBefore);
    EXPECT_EQ(fontLoads, fontBefore);
    EXPECT_EQ(soundLoads, soundBefore);

    EXPECT_FALSE(manager.tryGetSoundResident(core::resources::SoundKey{}).valid());
}

// ------------------------------------------------------------------------------------------------
// tryGetSoundResource(SoundHandle):
//  - invalid handle (SoundHandle{}) → nullptr
//  - valid index but NotLoaded → nullptr
//  - valid index but Failed (after failed load) → nullptr
//  - valid resident handle → non-null pointer, stable across repeated calls
// ------------------------------------------------------------------------------------------------
TEST_F(ResourceManagerTest, TryGetSoundResource_AllStateTransitions) {
    const auto env = makeEnv(false);

    core::resources::ResourceManager manager;
    initialize(manager, env);

    // Case 1: invalid handle → nullptr.
    EXPECT_EQ(manager.tryGetSoundResource(core::resources::SoundHandle{}), nullptr);

    // Case 2: valid cache index but NotLoaded → nullptr.
    // After init all sounds are NotLoaded. Fabricate a handle with a valid index.
    const auto clickKey = manager.findSound("core.sound.click");
    ASSERT_TRUE(clickKey.valid());
    EXPECT_EQ(manager.tryGetSoundResource(core::resources::SoundHandle{clickKey.index()}),
              nullptr);

    // Case 3: valid cache index but Failed → nullptr.
    const auto failKey = manager.findSound("core.sound.fail");
    ASSERT_TRUE(failKey.valid());

    failSound = true;
    const auto failResult = manager.tryGetSound(failKey);

    EXPECT_FALSE(failResult.valid()); // tryGetSound returns invalid on failure

    // The sound at failKey.index() is now in Failed state.
    EXPECT_EQ(manager.tryGetSoundResource(core::resources::SoundHandle{failKey.index()}),
              nullptr);

    // Case 4: resident handle → non-null, stable pointer.
    failSound = false;
    const auto okHandle = manager.tryGetSound(clickKey);
    ASSERT_TRUE(okHandle.valid());

    const auto* resource = manager.tryGetSoundResource(okHandle);
    EXPECT_NE(resource, nullptr);

    // Pointer stability: repeated calls return the same address.
    const auto* resource2 = manager.tryGetSoundResource(okHandle);
    EXPECT_EQ(resource, resource2);
}

// ------------------------------------------------------------------------------------------------
// preload + metrics:
//  - после init: 1 texture (missing), 1 font (missing), 0 sound
//  - preload secondary texture: metrics.textureCount растёт
//  - preload ok sound: metrics.soundCount растёт; tryGetSoundResident становится != nullptr
//  - preload failed sound не увеличивает metrics.soundCount (Failed != Resident)
// ------------------------------------------------------------------------------------------------
TEST_F(ResourceManagerTest, PreloadAndMetrics_AreConsistent) {
    const auto env = makeEnv(false);

    core::resources::ResourceManager manager;
    initialize(manager, env);

    {
        const auto m = manager.getMetrics();
        EXPECT_EQ(m.textureCount, 1u);
        EXPECT_EQ(m.fontCount, 1u);
        EXPECT_EQ(m.soundCount, 0u);
    }

    const auto secondaryTex = manager.findTexture("core.texture.secondary");
    ASSERT_TRUE(secondaryTex.valid());

    manager.preloadTexture(secondaryTex);

    {
        const auto m = manager.getMetrics();
        EXPECT_EQ(m.textureCount, 2u);
        EXPECT_EQ(m.fontCount, 1u);
        EXPECT_EQ(m.soundCount, 0u);
    }

    const auto okSound = manager.findSound("core.sound.click");
    ASSERT_TRUE(okSound.valid());

    manager.preloadSound(okSound);

    {
        const auto m = manager.getMetrics();
        EXPECT_EQ(m.textureCount, 2u);
        EXPECT_EQ(m.fontCount, 1u);
        EXPECT_EQ(m.soundCount, 1u);
    }

    const auto residentOk = manager.tryGetSoundResident(okSound);
    ASSERT_TRUE(residentOk.valid());

    const int before = soundLoads;

    // Resident-путь не должен пытаться грузить заново.
    const auto s2 = manager.tryGetSound(okSound);
    ASSERT_TRUE(s2.valid());
    EXPECT_EQ(s2, residentOk);
    EXPECT_EQ(soundLoads, before);

    const auto failSoundKey = manager.findSound("core.sound.fail");
    ASSERT_TRUE(failSoundKey.valid());

    failSound = true;
    manager.preloadSound(failSoundKey);

    {
        const auto m = manager.getMetrics();
        EXPECT_EQ(m.soundCount, 1u);
    }
}

// ------------------------------------------------------------------------------------------------
// preloadAll*ByRegistry():
//  - textures: грузит все текстуры (secondary один раз; missing уже resident)
//  - fonts: грузит все шрифты (default уже resident; secondary если присутствует)
//  - sounds: уважает soft-fail и запрет retry
// ------------------------------------------------------------------------------------------------
TEST_F(ResourceManagerTest, PreloadAllByRegistry_RespectsContracts) {
    const auto env = makeEnv(true);

    core::resources::ResourceManager manager;
    initialize(manager, env);

    ASSERT_EQ(textureLoads, 1);
    ASSERT_EQ(fontLoads, 1);
    ASSERT_EQ(soundLoads, 0);

    manager.preloadAllTexturesByRegistry();
    EXPECT_EQ(textureLoads, 2);

    manager.preloadAllFontsByRegistry();
    EXPECT_EQ(fontLoads, 2);

    failSound = true;
    manager.preloadAllSoundsByRegistry();
    EXPECT_EQ(soundLoads, 2);

    const auto okSound = manager.findSound("core.sound.click");
    ASSERT_TRUE(okSound.valid());
    EXPECT_TRUE(manager.tryGetSoundResident(okSound).valid());

    const auto failKey = manager.findSound("core.sound.fail");
    ASSERT_TRUE(failKey.valid());
    EXPECT_FALSE(manager.tryGetSoundResident(failKey).valid());

    // Повторный preloadAllSoundsByRegistry не должен делать retry на Failed.
    failSound = false;
    const int before = soundLoads;
    manager.preloadAllSoundsByRegistry();
    EXPECT_EQ(soundLoads, before);
}

// ------------------------------------------------------------------------------------------------
// setIoForbidden(true):
//  - resident ресурсы доступны без I/O
//  - любая попытка lazy-load нерезидентного ресурса должна panic-нуть
//
// Примечание: ASSERT_DEATH здесь неприменим — данный бинарник линкует
// log_panic_sink_throw.cpp, который реализует panic_sink через throw, а не exit().
// GTest death-test ожидает завершения дочернего процесса; исключение вместо этого
// вызывает "threw an exception" → FAILED. Используем expectPanic<F>().
// ------------------------------------------------------------------------------------------------
TEST_F(ResourceManagerTest, IoForbidden_DisallowsLazyLoadButAllowsResident) {
    const auto env = makeEnv(false);

    core::resources::ResourceManager manager;
    initialize(manager, env);

    const auto secondaryTex = manager.findTexture("core.texture.secondary");
    ASSERT_TRUE(secondaryTex.valid());

    const auto okSound = manager.findSound("core.sound.click");
    ASSERT_TRUE(okSound.valid());

    manager.setIoForbidden(true);

    // Fallback resident ресурсы должны оставаться доступными — без паники.
    EXPECT_NO_THROW((void)manager.getTexture(manager.missingTextureKey()));
    EXPECT_NO_THROW((void)manager.getFont(manager.missingFontKey()));

    // Lazy-load запрещён: каждый вызов должен panic-нуть.
    {
        const std::string msg = expectPanic([&]{ (void)manager.getTexture(secondaryTex); });
        EXPECT_NE(msg.find("getTexture"), std::string::npos)
            << "Ожидалась паника в getTexture. Panic msg: " << msg;
    }
    {
        const std::string msg = expectPanic([&]{ (void)manager.tryGetSound(okSound); });
        EXPECT_NE(msg.find("tryGetSound"), std::string::npos)
            << "Ожидалась паника в tryGetSound. Panic msg: " << msg;
    }
}

// ------------------------------------------------------------------------------------------------
// expect*Resident(): должен panic-нуть, если key валиден, но ресурс не resident.
// ------------------------------------------------------------------------------------------------
TEST_F(ResourceManagerTest, ExpectResident_PanicsWhenNotResident) {
    const auto env = makeEnv(true);

    core::resources::ResourceManager manager;
    initialize(manager, env);

    const auto secondaryTex = manager.findTexture("core.texture.secondary");
    ASSERT_TRUE(secondaryTex.valid());

    const auto secondaryFont = manager.findFont("core.font.secondary");
    ASSERT_TRUE(secondaryFont.valid());

    {
        const std::string msg = expectPanic([&]{ (void)manager.expectTextureResident(secondaryTex); });
        EXPECT_NE(msg.find("expectTextureResident"), std::string::npos)
            << "Ожидалась паника в expectTextureResident. Panic msg: " << msg;
    }
    {
        const std::string msg = expectPanic([&]{ (void)manager.expectFontResident(secondaryFont); });
        EXPECT_NE(msg.find("expectFontResident"), std::string::npos)
            << "Ожидалась паника в expectFontResident. Panic msg: " << msg;
    }
}

// ------------------------------------------------------------------------------------------------
// clearAll():
//  - сохраняет missing texture/font resident
//  - сбрасывает остальные кэши/состояния
//  - bump-ает cacheGeneration
//  - после clearAll expectResident(non-missing) снова должен panic-нуть
// ------------------------------------------------------------------------------------------------
TEST_F(ResourceManagerTest, ClearAll_ResetsCaches_KeepsFallback_BumpsGeneration) {
    const auto env = makeEnv(true);

    core::resources::ResourceManager manager;
    initialize(manager, env);

    const auto secondaryTex = manager.findTexture("core.texture.secondary");
    ASSERT_TRUE(secondaryTex.valid());

    const auto secondaryFont = manager.findFont("core.font.secondary");
    ASSERT_TRUE(secondaryFont.valid());

    const auto okSound = manager.findSound("core.sound.click");
    ASSERT_TRUE(okSound.valid());

    manager.preloadTexture(secondaryTex);
    manager.preloadFont(secondaryFont);
    manager.preloadSound(okSound);

    const auto genBefore = manager.cacheGeneration();
    ASSERT_NE(genBefore, 0u);

    manager.clearAll();

    const auto genAfter = manager.cacheGeneration();
    EXPECT_NE(genAfter, 0u);
    EXPECT_NE(genAfter, genBefore);

    {
        const auto m = manager.getMetrics();
        EXPECT_EQ(m.textureCount, 1u);
        EXPECT_EQ(m.fontCount, 1u);
        EXPECT_EQ(m.soundCount, 0u);
    }

    // Fallback должен оставаться resident — без паники.
    EXPECT_NO_THROW((void)manager.expectTextureResident(manager.missingTextureKey()));
    EXPECT_NO_THROW((void)manager.expectFontResident(manager.missingFontKey()));

    // Non-missing не должен быть resident после clearAll — каждый вызов panic-нет.
    {
        const std::string msg = expectPanic([&]{ (void)manager.expectTextureResident(secondaryTex); });
        EXPECT_NE(msg.find("expectTextureResident"), std::string::npos)
            << "Ожидалась паника в expectTextureResident после clearAll. Panic msg: " << msg;
    }
    {
        const std::string msg = expectPanic([&]{ (void)manager.expectFontResident(secondaryFont); });
        EXPECT_NE(msg.find("expectFontResident"), std::string::npos)
            << "Ожидалась паника в expectFontResident после clearAll. Panic msg: " << msg;
    }

    // Lazy-load по умолчанию разрешён после clearAll: ресурс должен перезагрузиться.
    const int texBefore = textureLoads;
    (void)manager.getTexture(secondaryTex);
    EXPECT_EQ(textureLoads, texBefore + 1);
}