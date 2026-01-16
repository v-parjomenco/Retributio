#include "pch.h"

#include "core/resources/registry/resource_registry.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#if defined(SFML1_TESTS)
    #include <format>
    #include <string>
#endif

#include "core/log/log_macros.h"
#include "core/resources/keys/stable_key.h"
#include "core/utils/file_loader.h"
#include "core/utils/json/json_document.h"

#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable : 5045) // /Qspectre mitigation warning treated as error under /WX
#endif

namespace core::resources {

    namespace {

        using Json = core::utils::json::json;
        using JsonValidator = core::utils::json::JsonValidator;
        using core::utils::FileLoader;

        constexpr std::string_view kRegistryModule = "ResourceRegistry";
        constexpr std::string_view kMissingTextureName = "core.texture.missing";
        constexpr std::string_view kMissingFontName = "core.font.default";

        struct StringViewHash {
            using is_transparent = void;

            [[nodiscard]] std::size_t operator()(std::string_view value) const noexcept {
                return std::hash<std::string_view>{}(value);
            }
        };

        struct StringViewEqual {
            using is_transparent = void;

            [[nodiscard]] bool operator()(std::string_view lhs, std::string_view rhs) const noexcept {
                return lhs == rhs;
            }
        };

#if defined(SFML1_TESTS)
        registry::test::StableKeyFn gStableKeyFn = computeStableKey64;
        registry::test::PanicHandler gPanicHandler = nullptr;
#endif

        [[nodiscard]] std::uint64_t stableKeyFor(std::string_view name) noexcept {
#if defined(SFML1_TESTS)
            if (gStableKeyFn != nullptr) {
                return gStableKeyFn(name);
            }
#endif
            return computeStableKey64(name);
        }

        template <typename... Args>
        [[noreturn]] void panic(const char* format, Args&&... args) {
#if defined(SFML1_TESTS)
            if (gPanicHandler != nullptr) {
                std::string message = std::vformat(format, std::make_format_args(args...));
                gPanicHandler(message);
                std::abort(); // если handler не бросил исключение — считаем panic фатальным
            }
#endif
            LOG_PANIC(core::log::cat::Resources, format, std::forward<Args>(args)...);
        }

        [[nodiscard]] bool isLowerAlpha(char c) noexcept {
            return (c >= 'a') && (c <= 'z');
        }

        [[nodiscard]] bool isLowerAlphaNumeric(char c) noexcept {
            return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        }

        [[nodiscard]] bool isValidCanonicalKey(std::string_view key) noexcept {
            if (key.empty()) {
                return false;
            }

            std::size_t segmentCount = 0u;
            std::size_t i = 0u;

            while (i < key.size()) {
                const char first = key[i];
                if (!isLowerAlpha(first)) {
                    return false;
                }

                ++segmentCount;
                ++i;

                while (i < key.size() && key[i] != '.') {
                    const char c = key[i];
                    if (!(isLowerAlphaNumeric(c) || c == '_' || c == '-')) {
                        return false;
                    }
                    ++i;
                }

                if (i == key.size()) {
                    break;
                }

                // key[i] == '.'
                ++i;
                if (i == key.size()) {
                    return false; // trailing dot
                }
            }

            return segmentCount >= 3u;
        }

        void validateCanonicalKey(std::string_view key) {
            if (!isValidCanonicalKey(key)) {
                panic("[ResourceRegistry] Invalid canonical key '{}'.", key);
            }
        }

        [[nodiscard]] bool readBooleanWithDefault(const Json& value,
                                                  std::string_view fieldName,
                                                  bool defaultValue,
                                                  std::string_view entryName) {
            const auto it = value.find(fieldName);
            if (it == value.end()) {
                return defaultValue;
            }
            if (!it->is_boolean()) {
                panic("[ResourceRegistry] Field '{}' for '{}' must be boolean.", fieldName, entryName);
            }
            return it->get<bool>();
        }

        [[nodiscard]] std::string_view requireStringField(const Json& value,
                                                          std::string_view fieldName,
                                                          std::string_view entryName,
                                                          std::string_view typeName) {
            const auto it = value.find(fieldName);
            if (it == value.end()) {
                panic("[ResourceRegistry] {} '{}' is missing required '{}' field.",
                      typeName,
                      entryName,
                      fieldName);
            }

            const auto* ptr = it->get_ptr<const std::string*>();
            if (ptr == nullptr) {
                panic("[ResourceRegistry] {} '{}' field '{}' must be a string.",
                      typeName,
                      entryName,
                      fieldName);
            }

            if (ptr->empty()) {
                panic("[ResourceRegistry] {} '{}' field '{}' must not be empty.",
                      typeName,
                      entryName,
                      fieldName);
            }

            return *ptr;
        }

        [[nodiscard]] std::optional<std::string_view> tryReadStringField(const Json& value,
                                                                         std::string_view fieldName) {
            const auto it = value.find(fieldName);
            if (it == value.end()) {
                return std::nullopt;
            }

            const auto* ptr = it->get_ptr<const std::string*>();
            if (ptr == nullptr || ptr->empty()) {
                return std::nullopt;
            }

            return std::string_view{*ptr};
        }

        template <typename Config>
        struct ResourceDefinition {
            std::string_view name{};       // interned canonical key
            std::string_view path{};       // interned path
            Config config{};               // type-specific config
            std::uint64_t stableKey = 0u;  // StableKey64 (computed after overrides)
            int layerPriority = 0;
            int loadOrder = 0;
            std::string_view sourceName{}; // diagnostics only
        };

        template <typename Config>
        using DefinitionMap = std::unordered_map<std::string_view,
                                                 ResourceDefinition<Config>,
                                                 StringViewHash,
                                                 StringViewEqual>;

        void ensureUniqueInSource(std::unordered_set<std::string_view, StringViewHash, StringViewEqual>& sourceKeys,
                                  std::string_view key) {
            const auto [_, inserted] = sourceKeys.emplace(key);
            if (!inserted) {
                panic("[ResourceRegistry] Duplicate canonical key '{}' within a single source file.", key);
            }
        }

        template <typename Config>
        void insertOrOverride(DefinitionMap<Config>& map, ResourceDefinition<Config> definition) {
            auto [it, inserted] = map.emplace(definition.name, definition);
            if (inserted) {
                return;
            }

            const auto& existing = it->second;

            if (definition.layerPriority > existing.layerPriority) {
                it->second = std::move(definition);
                return;
            }
            if (definition.layerPriority < existing.layerPriority) {
                return;
            }

            if (definition.loadOrder > existing.loadOrder) {
                it->second = std::move(definition);
                return;
            }
            if (definition.loadOrder < existing.loadOrder) {
                return;
            }

            panic("[ResourceRegistry] Duplicate canonical key '{}' with tied override policy: "
                  "sources '{}' and '{}' both have layerPriority={} and loadOrder={}.",
                  definition.name,
                  existing.sourceName,
                  definition.sourceName,
                  definition.layerPriority,
                  definition.loadOrder);
        }

        void parseRegistryHeader(const Json& data, std::string_view path) {
            const auto it = data.find("version");
            if (it == data.end()) {
                panic("[ResourceRegistry] Missing 'version' in {}.", path);
            }

            if (const auto* ptr = it->get_ptr<const Json::number_integer_t*>(); ptr != nullptr) {
                if (*ptr != 1) {
                    panic("[ResourceRegistry] Unsupported registry version {} in {}.", *ptr, path);
                }
                return;
            }

            if (const auto* ptr = it->get_ptr<const Json::number_unsigned_t*>(); ptr != nullptr) {
                if (*ptr != 1u) {
                    panic("[ResourceRegistry] Unsupported registry version {} in {}.", *ptr, path);
                }
                return;
            }

            panic("[ResourceRegistry] Invalid 'version' type in {}.", path);
        }

        const Json& requireObjectBlock(const Json& data, std::string_view key) {
            const auto it = data.find(key);
            if (it == data.end()) {
                panic("[ResourceRegistry] Missing required '{}' block.", key);
            }
            if (!it->is_object()) {
                panic("[ResourceRegistry] '{}' block must be an object.", key);
            }
            return *it;
        }

        void parseTextureBlock(const Json& textures,
                               const ResourceSource& source,
                               registry::StringPool& strings,
                               std::unordered_set<std::string_view, StringViewHash, StringViewEqual>& sourceKeys,
                               DefinitionMap<config::TextureResourceConfig>& out) {
            for (auto it = textures.begin(); it != textures.end(); ++it) {
                const std::string_view key = it.key();
                const Json& value = it.value();

                validateCanonicalKey(key);
                ensureUniqueInSource(sourceKeys, key);

                if (!value.is_object()) {
                    panic("[ResourceRegistry] Texture '{}' must be an object.", key);
                }

                const std::string_view path = requireStringField(value, "path", key, "Texture");

                if (!FileLoader::fileExists(std::string(path))) {
                    panic("[ResourceRegistry] Texture file '{}' not found for '{}'.", path, key);
                }

                config::TextureResourceConfig cfg{};
                cfg.smooth = readBooleanWithDefault(value, "smooth", false, key);
                cfg.repeated = readBooleanWithDefault(value, "repeated", false, key);
                cfg.generateMipmap = readBooleanWithDefault(value, "mipmap", false, key);

                ResourceDefinition<config::TextureResourceConfig> def{};
                def.name = strings.intern(key);
                def.path = strings.intern(path);
                def.config = cfg;
                def.layerPriority = source.layerPriority;
                def.loadOrder = source.loadOrder;
                def.sourceName = source.sourceName;

                insertOrOverride(out, std::move(def));
            }
        }

        void parseFontBlock(const Json& fonts,
                            const ResourceSource& source,
                            registry::StringPool& strings,
                            std::unordered_set<std::string_view, StringViewHash, StringViewEqual>& sourceKeys,
                            DefinitionMap<config::FontResourceConfig>& out) {
            for (auto it = fonts.begin(); it != fonts.end(); ++it) {
                const std::string_view key = it.key();
                const Json& value = it.value();

                validateCanonicalKey(key);
                ensureUniqueInSource(sourceKeys, key);

                if (!value.is_object()) {
                    panic("[ResourceRegistry] Font '{}' must be an object.", key);
                }

                const std::string_view path = requireStringField(value, "path", key, "Font");

                if (!FileLoader::fileExists(std::string(path))) {
                    panic("[ResourceRegistry] Font file '{}' not found for '{}'.", path, key);
                }

                config::FontResourceConfig cfg{};

                ResourceDefinition<config::FontResourceConfig> def{};
                def.name = strings.intern(key);
                def.path = strings.intern(path);
                def.config = cfg;
                def.layerPriority = source.layerPriority;
                def.loadOrder = source.loadOrder;
                def.sourceName = source.sourceName;

                insertOrOverride(out, std::move(def));
            }
        }

        void parseSoundBlock(const Json& sounds,
                             const ResourceSource& source,
                             registry::StringPool& strings,
                             std::unordered_set<std::string_view, StringViewHash, StringViewEqual>& sourceKeys,
                             DefinitionMap<config::SoundResourceConfig>& out) {
            for (auto it = sounds.begin(); it != sounds.end(); ++it) {
                const std::string_view key = it.key();
                const Json& value = it.value();

                validateCanonicalKey(key);
                ensureUniqueInSource(sourceKeys, key);

                if (!value.is_object()) {
                    LOG_WARN(core::log::cat::Resources,
                             "[ResourceRegistry] Sound '{}' must be an object. Skipping.",
                             key);
                    continue;
                }

                const auto pathOpt = tryReadStringField(value, "path");
                if (!pathOpt) {
                    LOG_WARN(core::log::cat::Resources,
                             "[ResourceRegistry] Sound '{}' missing 'path'. Skipping.",
                             key);
                    continue;
                }

                const std::string_view path = *pathOpt;

                if (!FileLoader::fileExists(std::string(path))) {
                    LOG_WARN(core::log::cat::Resources,
                             "[ResourceRegistry] Sound file '{}' not found for '{}'. Skipping.",
                             path,
                             key);
                    continue;
                }

                config::SoundResourceConfig cfg{};

                ResourceDefinition<config::SoundResourceConfig> def{};
                def.name = strings.intern(key);
                def.path = strings.intern(path);
                def.config = cfg;
                def.layerPriority = source.layerPriority;
                def.loadOrder = source.loadOrder;
                def.sourceName = source.sourceName;

                insertOrOverride(out, std::move(def));
            }
        }

        template <typename Config>
        void computeStableKeysAndValidateCollisions(DefinitionMap<Config>& defs,
                                                    std::unordered_map<std::uint64_t, std::string_view>& globalLookup) {
            for (auto& [_, def] : defs) {
                def.stableKey = stableKeyFor(def.name);

                auto [it, inserted] = globalLookup.emplace(def.stableKey, def.name);
                if (!inserted && it->second != def.name) {
                    panic("[ResourceRegistry] StableKey64 collision: '{}' and '{}' share {}.",
                          it->second,
                          def.name,
                          def.stableKey);
                }
            }
        }

        template <typename Entry, typename Config, typename Key>
        void finalizeEntries(const DefinitionMap<Config>& definitions,
                             std::vector<Entry>& entries,
                             std::vector<NameIndex>& nameIndex) {
            const std::size_t count = definitions.size();
            if (count > (static_cast<std::size_t>(Key::MaxIndex) + 1u)) {
                panic("[ResourceRegistry] Too many entries for this key type: {} (max {}).",
                      count,
                      static_cast<std::size_t>(Key::MaxIndex) + 1u);
            }

            std::vector<const ResourceDefinition<Config>*> sorted;
            sorted.reserve(count);
            for (const auto& [_, def] : definitions) {
                sorted.push_back(&def);
            }

            std::sort(sorted.begin(), sorted.end(), [](const auto* lhs, const auto* rhs) {
                return lhs->stableKey < rhs->stableKey;
            });

            entries.clear();
            entries.reserve(sorted.size());

            for (std::size_t i = 0; i < sorted.size(); ++i) {
                const auto& def = *sorted[i];

                Entry entry{};
                entry.key = Key::make(static_cast<std::uint32_t>(i));
                entry.stableKey = def.stableKey;
                entry.name = def.name;
                entry.path = def.path;
                entry.config = def.config;

                entries.push_back(std::move(entry));
            }

            nameIndex.clear();
            nameIndex.reserve(entries.size());
            for (std::size_t i = 0; i < entries.size(); ++i) {
                nameIndex.push_back(NameIndex{entries[i].name, static_cast<std::uint32_t>(i)});
            }

            std::sort(nameIndex.begin(), nameIndex.end(), [](const NameIndex& lhs, const NameIndex& rhs) {
                return lhs.name < rhs.name;
            });
        }

        template <typename Key>
        [[nodiscard]] Key findByNameIndex(const std::vector<NameIndex>& index,
                                          std::string_view name,
                                          const auto& entries) {
            const auto it = std::lower_bound(index.begin(),
                                             index.end(),
                                             name,
                                             [](const NameIndex& e, std::string_view v) {
                                                 return e.name < v;
                                             });
            if (it == index.end() || it->name != name) {
                return Key{};
            }

            const std::uint32_t idx = it->index;
            assert(idx < entries.size());
            return entries[idx].key;
        }

    } // namespace

    void ResourceRegistry::loadFromSources(std::span<const ResourceSource> sources) {
        mStrings = registry::StringPool{};

        mTextures.clear();
        mFonts.clear();
        mSounds.clear();
        mTextureNameIndex.clear();
        mFontNameIndex.clear();
        mSoundNameIndex.clear();

        mMissingTexture = TextureKey{};
        mMissingFont = FontKey{};

        DefinitionMap<config::TextureResourceConfig> textureDefinitions;
        DefinitionMap<config::FontResourceConfig> fontDefinitions;
        DefinitionMap<config::SoundResourceConfig> soundDefinitions;

        for (const auto& source : sources) {
            const auto content = FileLoader::loadTextFile(source.path);
            if (!content) {
                panic("[ResourceRegistry] Failed to load registry file '{}'.", source.path);
            }

            const Json data = core::utils::json::parseAndValidateCritical(
                *content,
                source.path,
                kRegistryModule,
                {JsonValidator::KeyRule{"version",
                                        {Json::value_t::number_integer, Json::value_t::number_unsigned},
                                        true},
                 JsonValidator::KeyRule{"textures", {Json::value_t::object}, true},
                 JsonValidator::KeyRule{"fonts", {Json::value_t::object}, true},
                 JsonValidator::KeyRule{"sounds", {Json::value_t::object}, true}});

            parseRegistryHeader(data, source.path);

            const Json& textures = requireObjectBlock(data, "textures");
            const Json& fonts = requireObjectBlock(data, "fonts");
            const Json& sounds = requireObjectBlock(data, "sounds");

            std::unordered_set<std::string_view, StringViewHash, StringViewEqual> sourceKeys;
            sourceKeys.reserve(textures.size() + fonts.size() + sounds.size());

            parseTextureBlock(textures, source, mStrings, sourceKeys, textureDefinitions);
            parseFontBlock(fonts, source, mStrings, sourceKeys, fontDefinitions);
            parseSoundBlock(sounds, source, mStrings, sourceKeys, soundDefinitions);
        }

        const std::size_t totalDefs =
            textureDefinitions.size() + fontDefinitions.size() + soundDefinitions.size();

        std::unordered_map<std::uint64_t, std::string_view> stableKeyGlobal;
        stableKeyGlobal.reserve(totalDefs);

        computeStableKeysAndValidateCollisions(textureDefinitions, stableKeyGlobal);
        computeStableKeysAndValidateCollisions(fontDefinitions, stableKeyGlobal);
        computeStableKeysAndValidateCollisions(soundDefinitions, stableKeyGlobal);

        finalizeEntries<TextureEntry, config::TextureResourceConfig, TextureKey>(
            textureDefinitions,
            mTextures,
            mTextureNameIndex);
        finalizeEntries<FontEntry, config::FontResourceConfig, FontKey>(
            fontDefinitions,
            mFonts,
            mFontNameIndex);
        finalizeEntries<SoundEntry, config::SoundResourceConfig, SoundKey>(
            soundDefinitions,
            mSounds,
            mSoundNameIndex);

        mMissingTexture = findTextureByName(kMissingTextureName);
        if (!mMissingTexture.valid()) {
            panic("[ResourceRegistry] Missing required fallback texture '{}'.", kMissingTextureName);
        }

        mMissingFont = findFontByName(kMissingFontName);
        if (!mMissingFont.valid()) {
            panic("[ResourceRegistry] Missing required fallback font '{}'.", kMissingFontName);
        }

        mStrings.clearLookup();
    }

    const TextureEntry& ResourceRegistry::getTexture(TextureKey key) const noexcept {
        assert(key.valid());
        const std::uint32_t index = key.index();
        assert(index < mTextures.size());
        return mTextures[index];
    }

    const FontEntry& ResourceRegistry::getFont(FontKey key) const noexcept {
        assert(key.valid());
        const std::uint32_t index = key.index();
        assert(index < mFonts.size());
        return mFonts[index];
    }

    const SoundEntry* ResourceRegistry::tryGetSound(SoundKey key) const noexcept {
        if (!key.valid()) {
            return nullptr;
        }

        const std::uint32_t index = key.index();
        if (index >= mSounds.size()) {
            return nullptr;
        }

        return &mSounds[index];
    }

    TextureKey ResourceRegistry::findTextureByName(std::string_view name) const {
        return findByNameIndex<TextureKey>(mTextureNameIndex, name, mTextures);
    }

    TextureKey ResourceRegistry::findTextureByStableKey(std::uint64_t stableKey) const {
        const auto it = std::lower_bound(
            mTextures.begin(),
            mTextures.end(),
            stableKey,
            [](const TextureEntry& entry, std::uint64_t value) { return entry.stableKey < value; });

        if (it == mTextures.end() || it->stableKey != stableKey) {
            return TextureKey{};
        }

        return it->key;
    }

    FontKey ResourceRegistry::findFontByName(std::string_view name) const {
        return findByNameIndex<FontKey>(mFontNameIndex, name, mFonts);
    }

    SoundKey ResourceRegistry::findSoundByName(std::string_view name) const {
        return findByNameIndex<SoundKey>(mSoundNameIndex, name, mSounds);
    }

    TextureKey ResourceRegistry::missingTextureKey() const noexcept {
        return mMissingTexture;
    }

    FontKey ResourceRegistry::missingFontKey() const noexcept {
        return mMissingFont;
    }

    std::size_t ResourceRegistry::textureCount() const noexcept {
        return mTextures.size();
    }

    std::size_t ResourceRegistry::fontCount() const noexcept {
        return mFonts.size();
    }

    std::size_t ResourceRegistry::soundCount() const noexcept {
        return mSounds.size();
    }

#if defined(SFML1_TESTS)
    namespace registry::test {
        void setStableKeyFn(StableKeyFn fn) noexcept {
            gStableKeyFn = fn;
        }

        void resetStableKeyFn() noexcept {
            gStableKeyFn = computeStableKey64;
        }

        void setPanicHandler(PanicHandler handler) noexcept {
            gPanicHandler = handler;
        }

        void resetPanicHandler() noexcept {
            gPanicHandler = nullptr;
        }
    } // namespace registry::test
#endif // defined(SFML1_TESTS)

} // namespace core::resources

#if defined(_MSC_VER)
    #pragma warning(pop)
#endif