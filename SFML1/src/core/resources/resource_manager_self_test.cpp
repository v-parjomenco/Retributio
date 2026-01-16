// ================================================================================================
// File: core/resources/resource_manager_self_test.cpp
// Purpose: Self-tests for ResourceManager key-based API (SFML1_TESTS only).
// Notes:
//  - This TU owns its own SelfTestRunner and does NOT define any aggregator run().
// ================================================================================================
#include "pch.h"

#if defined(SFML1_TESTS)

#include <array>
#include <cassert>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "core/resources/resource_manager.h"
#include "core/utils/file_loader.h"

namespace core::resources::self_test {

    namespace {

        struct LoadCounters {
            int texture = 0;
            int font = 0;
            int sound = 0;
        };

        LoadCounters* gCounters = nullptr;

        bool gFailSoundByPath = false;

        bool textureStub(types::TextureResource&, std::string_view) {
            if (gCounters != nullptr) {
                ++gCounters->texture;
            }
            return true;
        }

        bool fontStub(types::FontResource&, std::string_view) {
            if (gCounters != nullptr) {
                ++gCounters->font;
            }
            return true;
        }

        bool soundStub(types::SoundBufferResource&, std::string_view path) {
            if (gCounters != nullptr) {
                ++gCounters->sound;
            }
            if (gFailSoundByPath && path.find("fail") != std::string_view::npos) {
                return false;
            }
            return true;
        }

        struct LoaderGuard {
            LoaderGuard() {
                resource_manager::test::setTextureLoadFn(textureStub);
                resource_manager::test::setFontLoadFn(fontStub);
                resource_manager::test::setSoundLoadFn(soundStub);
            }

            ~LoaderGuard() {
                resource_manager::test::resetTextureLoadFn();
                resource_manager::test::resetFontLoadFn();
                resource_manager::test::resetSoundLoadFn();
            }
        };

        std::filesystem::path makeTempDir(std::string_view name) {
            static std::size_t counter = 0u;

            std::filesystem::path base = std::filesystem::temp_directory_path();
            base /= "sfml1_resource_manager_tests";
            base /= std::string(name) + "_" + std::to_string(counter++);
            std::filesystem::create_directories(base);
            return base;
        }

        void writeTextFileOrFail(const std::filesystem::path& path, std::string_view content) {
            const bool ok = core::utils::FileLoader::writeTextFileAtomic(path, content);
            assert(ok);
        }

        std::string makeRegistryJson(std::string_view textures,
                                     std::string_view fonts,
                                     std::string_view sounds) {
            std::string json;
            json.reserve(textures.size() + fonts.size() + sounds.size() + 64u);
            json += "{\"version\":1,\"textures\":";
            json += textures;
            json += ",\"fonts\":";
            json += fonts;
            json += ",\"sounds\":";
            json += sounds;
            json += "}";
            return json;
        }

        ResourceSource makeSource(const std::filesystem::path& path,
                                  int layerPriority,
                                  int loadOrder,
                                  std::string_view name) {
            ResourceSource source{};
            source.path = path.generic_string();
            source.layerPriority = layerPriority;
            source.loadOrder = loadOrder;
            source.sourceName = std::string(name);
            return source;
        }

        struct TestEnv {
            std::filesystem::path dir;
            std::filesystem::path missingTexturePath;
            std::filesystem::path secondaryTexturePath;
            std::filesystem::path defaultFontPath;
            std::filesystem::path soundOkPath;
            std::filesystem::path soundFailPath;
            std::filesystem::path jsonPath;
        };

        TestEnv setupEnv(std::string_view name) {
            TestEnv env{};
            env.dir = makeTempDir(name);

            env.missingTexturePath = env.dir / "missing.png";
            env.secondaryTexturePath = env.dir / "secondary.png";
            env.defaultFontPath = env.dir / "default.ttf";
            env.soundOkPath = env.dir / "click.wav";
            env.soundFailPath = env.dir / "fail.wav";

            // Registry v1 тесты традиционно создают файлы, 
            // чтобы пройти validate-on-write/IO checks.
            writeTextFileOrFail(env.missingTexturePath, "tex_missing");
            writeTextFileOrFail(env.secondaryTexturePath, "tex_secondary");
            writeTextFileOrFail(env.defaultFontPath, "font");
            writeTextFileOrFail(env.soundOkPath, "sound_ok");
            writeTextFileOrFail(env.soundFailPath, "sound_fail");

            const std::string textures =
                "{\"core.texture.missing\":{\"path\":\"" +
                env.missingTexturePath.generic_string() +
                "\"},\"core.texture.secondary\":{\"path\":\"" +
                env.secondaryTexturePath.generic_string() + "\"}}";

            const std::string fonts =
                "{\"core.font.default\":{\"path\":\"" + 
                env.defaultFontPath.generic_string() + "\"}}";

            const std::string sounds =
                "{\"core.sound.click\":{\"path\":\"" + 
                env.soundOkPath.generic_string() +
                "\"},\"core.sound.fail\":{\"path\":\"" + 
                env.soundFailPath.generic_string() + "\"}}";

            env.jsonPath = env.dir / "resources.json";
            writeTextFileOrFail(env.jsonPath, makeRegistryJson(textures, fonts, sounds));

            return env;
        }

        void testInitializeAndMissingKeys() {
            const TestEnv env = setupEnv("init");

            LoadCounters counters{};
            gCounters = &counters;
            gFailSoundByPath = false;

            LoaderGuard guard{};

            ResourceManager manager;
            const std::array<ResourceSource, 1> sources{makeSource(env.jsonPath, 0, 0, "core")};
            manager.initialize(sources);

            const auto& reg = manager.registry();
            assert(reg.textureCount() == 2u);
            assert(reg.fontCount() == 1u);
            assert(reg.soundCount() == 2u);

            assert(reg.missingTextureKey().valid());
            assert(reg.missingFontKey().valid());

            // Public getters must be consistent after initialize().
            assert(manager.missingTextureKey() == reg.missingTextureKey());
            assert(manager.missingFontKey() == reg.missingFontKey());
        }

        void testLoadOnceTextureAndFont() {
            const TestEnv env = setupEnv("load_once");

            LoadCounters counters{};
            gCounters = &counters;
            gFailSoundByPath = false;

            LoaderGuard guard{};

            ResourceManager manager;
            const std::array<ResourceSource, 1> sources{makeSource(env.jsonPath, 0, 0, "core")};
            manager.initialize(sources);

            // Texture: load once
            const TextureKey missingKey = manager.missingTextureKey();
            const auto& t1 = manager.getTexture(missingKey);
            const auto& t2 = manager.getTexture(missingKey);
            assert(&t1 == &t2);
            assert(counters.texture == 1);

            // invalid/out-of-range => fallback texture
            const auto& tf = manager.getTexture(missingKey);
            const auto& ti = manager.getTexture(TextureKey{});
            assert(&tf == &ti);

            // Доп. проверка: эти вызовы не должны триггерить повторную загрузку.
            const int beforeTex = counters.texture;
            (void) manager.getTexture(missingKey);
            (void) manager.getTexture(TextureKey{});
            assert(counters.texture == beforeTex);

            // Font: load once
            const FontKey missingFont = manager.missingFontKey();
            const auto& f1 = manager.getFont(missingFont);
            const auto& f2 = manager.getFont(missingFont);
            assert(&f1 == &f2);
            assert(counters.font == 1);

            // invalid/out-of-range => fallback font
            const auto& ff = manager.getFont(missingFont);
            const auto& fi = manager.getFont(FontKey{});
            assert(&ff == &fi);

            // Доп. проверка: эти вызовы не должны триггерить повторную загрузку.
            const int beforeFont = counters.font;
            (void) manager.getFont(missingFont);
            (void) manager.getFont(FontKey{});
            assert(counters.font == beforeFont);

            // invalid sound key => nullptr
            const auto* invalidSound = manager.tryGetSound(SoundKey{});
            assert(invalidSound == nullptr);
        }

        void testFindTextureLoadsSecondary() {
            const TestEnv env = setupEnv("find_texture");

            LoadCounters counters{};
            gCounters = &counters;
            gFailSoundByPath = false;

            LoaderGuard guard{};

            ResourceManager manager;
            const std::array<ResourceSource, 1> sources{makeSource(env.jsonPath, 0, 0, "core")};
            manager.initialize(sources);

            const TextureKey secondary = manager.findTexture("core.texture.secondary");
            assert(secondary.valid());

            const auto& a = manager.getTexture(secondary);
            const auto& b = manager.getTexture(secondary);
            assert(&a == &b);

            // Должна быть ровно одна загрузка: secondary грузится один раз, 
            // второй вызов берётся из кэша.
            assert(counters.texture == 1);
        }

        void testNoRetryAfterSoundFail() {
            const TestEnv env = setupEnv("no_retry_sound");

            LoadCounters counters{};
            gCounters = &counters;
            gFailSoundByPath = false;

            LoaderGuard guard{};

            ResourceManager manager;
            const std::array<ResourceSource, 1> sources{makeSource(env.jsonPath, 0, 0, "core")};
            manager.initialize(sources);

            const SoundKey failKey = manager.registry().findSoundByName("core.sound.fail");
            assert(failKey.valid());

            counters.sound = 0;
            gFailSoundByPath = true;

            const auto* s1 = manager.tryGetSound(failKey);
            const auto* s2 = manager.tryGetSound(failKey);

            assert(s1 == nullptr);
            assert(s2 == nullptr);

            // Soft-fail must be attempted only once for this run.
            assert(counters.sound == 1);

            gFailSoundByPath = false;
        }

        void run() {
            testInitializeAndMissingKeys();
            testLoadOnceTextureAndFont();
            testFindTextureLoadsSecondary();
            testNoRetryAfterSoundFail();

            gCounters = nullptr;
            gFailSoundByPath = false;
        }

    } // namespace

} // namespace core::resources::self_test

namespace {

    struct SelfTestRunner {
        SelfTestRunner() {
            core::resources::self_test::run();
        }
    } gSelfTestRunner;

} // namespace

#endif // defined(SFML1_TESTS)