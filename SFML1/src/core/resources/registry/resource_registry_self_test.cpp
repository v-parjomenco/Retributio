// ================================================================================================
// File: core/resources/registry/resource_registry_self_test.cpp
// Purpose: Self-tests for ResourceRegistry (SFML1_TESTS only).
// Notes:
//  - This TU must NOT define core::resources::registry::self_test::run() to avoid LNK2005.
//    A single aggregator TU (e.g. string_pool_self_test.cpp) should call runResourceRegistry().
// ================================================================================================
#include "pch.h"

#if defined(SFML1_TESTS)

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/resources/keys/stable_key.h"
#include "core/resources/registry/resource_registry.h"
#include "core/utils/file_loader.h"

namespace core::resources::registry::self_test {

    namespace {

        struct PanicException : std::exception {
            std::string message;

            explicit PanicException(std::string messageIn) : message(std::move(messageIn)) {
            }

            const char* what() const noexcept override {
                return message.c_str();
            }
        };

        void panicToException(std::string_view message) {
            throw PanicException(std::string(message));
        }

        struct PanicGuard {
            PanicGuard() {
                core::resources::registry::test::setPanicHandler(&panicToException);
            }

            ~PanicGuard() {
                core::resources::registry::test::resetPanicHandler();
            }
        };

        struct StableKeyGuard {
            explicit StableKeyGuard(core::resources::registry::test::StableKeyFn fn) {
                core::resources::registry::test::setStableKeyFn(fn);
            }

            ~StableKeyGuard() {
                core::resources::registry::test::resetStableKeyFn();
            }
        };

        std::filesystem::path makeTempDir(std::string_view name) {
            static std::size_t counter = 0u;

            std::filesystem::path base = std::filesystem::temp_directory_path();
            base /= "sfml1_registry_tests";
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

        template <typename Func>
        void expectPanic(Func&& fn) {
            PanicGuard guard{};
            bool threw = false;
            try {
                fn();
            } catch (const PanicException&) {
                threw = true;
            }
            assert(threw);
        }

        void testLoadValidJson() {
            const auto dir = makeTempDir("valid");
            const auto texturePath = dir / "missing_texture.png";
            const auto fontPath = dir / "default.ttf";
            const auto soundPath = dir / "click.wav";

            writeTextFileOrFail(texturePath, "texture");
            writeTextFileOrFail(fontPath, "font");
            writeTextFileOrFail(soundPath, "sound");

            const std::string textureEntry =
                "{\"core.texture.missing\":{\"path\":\"" +
                texturePath.generic_string() +
                "\",\"smooth\":false,\"repeated\":false,\"mipmap\":false}}";
            const std::string fontEntry =
                "{\"core.font.default\":{\"path\":\"" + fontPath.generic_string() + "\"}}";
            const std::string soundEntry =
                "{\"core.sound.click\":{\"path\":\"" + soundPath.generic_string() + "\"}}";

            const std::string json = makeRegistryJson(textureEntry, fontEntry, soundEntry);
            const auto jsonPath = dir / "resources.json";
            writeTextFileOrFail(jsonPath, json);

            ResourceRegistry registry;
            const std::vector<ResourceSource> sources{makeSource(jsonPath, 0, 0, "core")};
            registry.loadFromSources(sources);

            assert(registry.textureCount() == 1u);
            assert(registry.fontCount() == 1u);
            assert(registry.soundCount() == 1u);

            // findTextureByName + runtime getTexture
            const TextureKey missingKey = registry.findTextureByName("core.texture.missing");
            assert(missingKey.valid());
            const auto& missingEntry = registry.getTexture(missingKey);
            assert(missingEntry.name == "core.texture.missing");

            // findTextureByStableKey
            const std::uint64_t stableKey =
                core::resources::computeStableKey64("core.texture.missing");
            const TextureKey stableLookup = registry.findTextureByStableKey(stableKey);
            assert(stableLookup.valid());
            assert(stableLookup == missingEntry.key);

            // findFontByName + runtime getFont
            const FontKey fontKey = registry.findFontByName("core.font.default");
            assert(fontKey.valid());
            const auto& fontEntryLoaded = registry.getFont(fontKey);
            assert(fontEntryLoaded.name == "core.font.default");

            // findSoundByName + runtime tryGetSound
            const SoundKey soundKey = registry.findSoundByName("core.sound.click");
            assert(soundKey.valid());
            const auto* soundEntryLoaded = registry.tryGetSound(soundKey);
            assert(soundEntryLoaded != nullptr);
            assert(soundEntryLoaded->name == "core.sound.click");
        }

        void testDuplicateWithinSourcePanics() {
            const auto dir = makeTempDir("dup_source");
            const auto texturePath = dir / "missing_texture.png";
            const auto fontPath = dir / "default.ttf";

            writeTextFileOrFail(texturePath, "texture");
            writeTextFileOrFail(fontPath, "font");

            const std::string textureEntry =
                "{\"core.texture.missing\":{\"path\":\"" +
                texturePath.generic_string() + "\"}}";
            const std::string fontEntry =
                "{\"core.texture.missing\":{\"path\":\"" +
                fontPath.generic_string() + "\"}}";
            const std::string soundEntry = "{}";

            const std::string json = makeRegistryJson(textureEntry, fontEntry, soundEntry);
            const auto jsonPath = dir / "resources.json";
            writeTextFileOrFail(jsonPath, json);

            const std::vector<ResourceSource> sources{makeSource(jsonPath, 0, 0, "core")};

            expectPanic([&]() {
                ResourceRegistry registry;
                registry.loadFromSources(sources);
            });
        }

        void testDuplicateAcrossSourcesOverride() {
            const auto dir = makeTempDir("override");
            const auto coreTexturePath = dir / "missing_core.png";
            const auto modTexturePath = dir / "missing_mod.png";
            const auto fontPath = dir / "default.ttf";

            writeTextFileOrFail(coreTexturePath, "core_tex");
            writeTextFileOrFail(modTexturePath, "mod_tex");
            writeTextFileOrFail(fontPath, "font");

            const std::string coreTextures =
                "{\"core.texture.missing\":{\"path\":\"" +
                coreTexturePath.generic_string() + "\"}}";
            const std::string coreFonts =
                "{\"core.font.default\":{\"path\":\"" +
                fontPath.generic_string() + "\"}}";
            const std::string emptySounds = "{}";

            const std::string coreJson = makeRegistryJson(coreTextures, coreFonts, emptySounds);
            const auto coreJsonPath = dir / "core.json";
            writeTextFileOrFail(coreJsonPath, coreJson);

            const std::string modTextures =
                "{\"core.texture.missing\":{\"path\":\"" +
                modTexturePath.generic_string() + "\"}}";
            const std::string modFonts = "{}";
            const std::string modJson = makeRegistryJson(modTextures, modFonts, emptySounds);
            const auto modJsonPath = dir / "mod.json";
            writeTextFileOrFail(modJsonPath, modJson);

            const std::vector<ResourceSource> sources{
                makeSource(coreJsonPath, 0, 0, "core"),
                makeSource(modJsonPath, 3000, 0, "mod")};

            ResourceRegistry registry;
            registry.loadFromSources(sources);

            const TextureKey key = registry.findTextureByName("core.texture.missing");
            assert(key.valid());

            const auto& entry = registry.getTexture(key);
            assert(entry.path == modTexturePath.generic_string());
        }

        // Tie в override-политике (layerPriority/loadOrder совпали) => PANIC.
        void testOverrideTiePanics() {
            const auto dir = makeTempDir("override_tie");
            const auto texA = dir / "a.png";
            const auto texB = dir / "b.png";
            const auto fontPath = dir / "default.ttf";

            writeTextFileOrFail(texA, "a");
            writeTextFileOrFail(texB, "b");
            writeTextFileOrFail(fontPath, "font");

            const std::string texturesA =
                "{\"core.texture.missing\":{\"path\":\"" + texA.generic_string() + "\"}}";
            const std::string fontsA =
                "{\"core.font.default\":{\"path\":\"" + fontPath.generic_string() + "\"}}";
            const std::string soundsA = "{}";
            const auto jsonAPath = dir / "a.json";
            writeTextFileOrFail(jsonAPath, makeRegistryJson(texturesA, fontsA, soundsA));

            const std::string texturesB =
                "{\"core.texture.missing\":{\"path\":\"" + texB.generic_string() + "\"}}";
            const std::string fontsB =
                "{\"core.font.default\":{\"path\":\"" + fontPath.generic_string() + "\"}}";
            const std::string soundsB = "{}";
            const auto jsonBPath = dir / "b.json";
            writeTextFileOrFail(jsonBPath, makeRegistryJson(texturesB, fontsB, soundsB));

            // Одинаковые layerPriority и loadOrder => tie => PANIC.
            const std::vector<ResourceSource> sources{
                makeSource(jsonAPath, 0, 0, "A"),
                makeSource(jsonBPath, 0, 0, "B")};

            expectPanic([&]() {
                ResourceRegistry registry;
                registry.loadFromSources(sources);
            });
        }

        void testInvalidCanonicalKeyPanics() {
            const auto dir = makeTempDir("invalid_key");
            const auto texturePath = dir / "missing_texture.png";
            const auto fontPath = dir / "default.ttf";

            writeTextFileOrFail(texturePath, "texture");
            writeTextFileOrFail(fontPath, "font");

            const std::string textures =
                "{\"Core.Texture.Missing\":{\"path\":\"" +
                texturePath.generic_string() + "\"}}";
            const std::string fonts =
                "{\"core.font.default\":{\"path\":\"" +
                fontPath.generic_string() + "\"}}";
            const std::string sounds = "{}";

            const std::string json = makeRegistryJson(textures, fonts, sounds);
            const auto jsonPath = dir / "resources.json";
            writeTextFileOrFail(jsonPath, json);

            const std::vector<ResourceSource> sources{makeSource(jsonPath, 0, 0, "core")};

            expectPanic([&]() {
                ResourceRegistry registry;
                registry.loadFromSources(sources);
            });
        }

        std::uint64_t constantStableKey(std::string_view) noexcept {
            return 42u;
        }

        void testStableKeyCollisionPanics() {
            const auto dir = makeTempDir("collision");
            const auto texturePathA = dir / "a.png";
            const auto texturePathB = dir / "b.png";
            const auto fontPath = dir / "default.ttf";

            writeTextFileOrFail(texturePathA, "a");
            writeTextFileOrFail(texturePathB, "b");
            writeTextFileOrFail(fontPath, "font");

            const std::string textures =
                "{\"core.texture.missing\":{\"path\":\"" +
                texturePathA.generic_string() +
                "\"},\"core.texture.other\":{\"path\":\"" +
                texturePathB.generic_string() + "\"}}";
            const std::string fonts =
                "{\"core.font.default\":{\"path\":\"" + fontPath.generic_string() + "\"}}";
            const std::string sounds = "{}";

            const std::string json = makeRegistryJson(textures, fonts, sounds);
            const auto jsonPath = dir / "resources.json";
            writeTextFileOrFail(jsonPath, json);

            const std::vector<ResourceSource> sources{makeSource(jsonPath, 0, 0, "core")};

            StableKeyGuard stableKeyGuard(&constantStableKey);

            expectPanic([&]() {
                ResourceRegistry registry;
                registry.loadFromSources(sources);
            });
        }

        // Детерминизм не должен зависеть от порядка ключей в JSON.
        void testDeterminismIgnoresJsonKeyOrder() {
            const auto dir = makeTempDir("determinism_order");
            const auto texMissing = dir / "missing_texture.png";
            const auto texSecondary = dir / "secondary.png";
            const auto fontPath = dir / "default.ttf";

            writeTextFileOrFail(texMissing, "m");
            writeTextFileOrFail(texSecondary, "s");
            writeTextFileOrFail(fontPath, "font");

            // JSON #1: missing затем secondary
            const std::string textures1 =
                "{\"core.texture.missing\":{\"path\":\"" + 
                texMissing.generic_string() +
                "\"},\"core.texture.secondary\":{\"path\":\"" + 
                texSecondary.generic_string() + "\"}}";
            const std::string fonts = "{\"core.font.default\":{\"path\":\"" + 
                fontPath.generic_string() + "\"}}";
            const std::string sounds = "{}";
            const auto json1Path = dir / "one.json";
            writeTextFileOrFail(json1Path, makeRegistryJson(textures1, fonts, sounds));

            // JSON #2: secondary затем missing (порядок ключей поменяли)
            const std::string textures2 =
                "{\"core.texture.secondary\":{\"path\":\"" + 
                texSecondary.generic_string() +
                "\"},\"core.texture.missing\":{\"path\":\"" + 
                texMissing.generic_string() + "\"}}";
            const auto json2Path = dir / "two.json";
            writeTextFileOrFail(json2Path, makeRegistryJson(textures2, fonts, sounds));

            ResourceRegistry reg1;
            const std::array<ResourceSource, 1> sources1{makeSource(json1Path, 0, 0, "one")};
            reg1.loadFromSources(sources1);

            ResourceRegistry reg2;
            const std::array<ResourceSource, 1> sources2{makeSource(json2Path, 0, 0, "two")};
            reg2.loadFromSources(sources2);

            const TextureKey m1 = reg1.findTextureByName("core.texture.missing");
            const TextureKey s1 = reg1.findTextureByName("core.texture.secondary");
            const TextureKey m2 = reg2.findTextureByName("core.texture.missing");
            const TextureKey s2 = reg2.findTextureByName("core.texture.secondary");

            assert(m1.valid() && s1.valid() && m2.valid() && s2.valid());

            // Индексы должны совпасть, несмотря на порядок ключей в JSON.
            assert(m1.index() == m2.index());
            assert(s1.index() == s2.index());
        }

        void testDeterminism() {
            const auto dir = makeTempDir("determinism");
            const auto texturePathA = dir / "a.png";
            const auto texturePathB = dir / "b.png";
            const auto fontPath = dir / "default.ttf";

            writeTextFileOrFail(texturePathA, "a");
            writeTextFileOrFail(texturePathB, "b");
            writeTextFileOrFail(fontPath, "font");

            const std::string textures =
                "{\"core.texture.missing\":{\"path\":\"" +
                texturePathA.generic_string() +
                "\"},\"core.texture.secondary\":{\"path\":\"" +
                texturePathB.generic_string() + "\"}}";
            const std::string fonts =
                "{\"core.font.default\":{\"path\":\"" +
                fontPath.generic_string() + "\"}}";
            const std::string sounds = "{}";

            const std::string json = makeRegistryJson(textures, fonts, sounds);
            const auto jsonPath = dir / "resources.json";
            writeTextFileOrFail(jsonPath, json);

            const std::vector<ResourceSource> sources{makeSource(jsonPath, 0, 0, "core")};

            std::uint32_t baselineMissingIndex = 0u;
            std::uint32_t baselineSecondaryIndex = 0u;

            for (int i = 0; i < 100; ++i) {
                ResourceRegistry registry;
                registry.loadFromSources(sources);

                const TextureKey missingKey =
                    registry.findTextureByName("core.texture.missing");
                const TextureKey secondaryKey =
                    registry.findTextureByName("core.texture.secondary");

                assert(missingKey.valid());
                assert(secondaryKey.valid());

                if (i == 0) {
                    baselineMissingIndex = missingKey.index();
                    baselineSecondaryIndex = secondaryKey.index();
                } else {
                    assert(missingKey.index() == baselineMissingIndex);
                    assert(secondaryKey.index() == baselineSecondaryIndex);
                }
            }
        }

    } // namespace

    void runResourceRegistry() {
        testLoadValidJson();
        testDuplicateWithinSourcePanics();
        testDuplicateAcrossSourcesOverride();
        testOverrideTiePanics();
        testInvalidCanonicalKeyPanics();
        testStableKeyCollisionPanics();
        testDeterminismIgnoresJsonKeyOrder();
        testDeterminism();
    }

} // namespace core::resources::registry::self_test

#endif // defined(SFML1_TESTS)