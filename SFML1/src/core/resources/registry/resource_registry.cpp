#include "pch.h"

#include "core/resources/registry/resource_registry.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "core/log/log_macros.h"
#include "core/resources/keys/stable_key.h"
#include "core/utils/file_loader.h"
#include "core/utils/json/json_document.h"

#if defined(_MSC_VER)
    // /Qspectre — информационное предупреждение, в тестовом коде неустранимо.
    #pragma warning(push)
    #pragma warning(disable : 5045)
#endif

namespace core::resources {

    namespace {

        using Json          = core::utils::json::json;
        using JsonValidator = core::utils::json::JsonValidator;
        using core::utils::FileLoader;

        constexpr std::string_view kRegistryModule     = "ResourceRegistry";
        constexpr std::string_view kMissingTextureName = "core.texture.missing";
        constexpr std::string_view kMissingFontName    = "core.font.default";

        struct StringViewHash {
            using is_transparent = void;

            [[nodiscard]] std::size_t operator()(std::string_view v) const noexcept {
                return std::hash<std::string_view>{}(v);
            }
        };

        struct StringViewEqual {
            using is_transparent = void;

            [[nodiscard]] bool operator()(
                std::string_view lhs,
                std::string_view rhs) const noexcept {
                return lhs == rhs;
            }
        };

        using StableKey       = std::uint64_t;
        using StableKeyLookup = std::unordered_map<StableKey, std::string_view>;
        using SourceKeySet    =
            std::unordered_set<std::string_view, StringViewHash, StringViewEqual>;

        [[nodiscard]] bool isLowerAlpha(char c) noexcept {
            return (c >= 'a') && (c <= 'z');
        }

        [[nodiscard]] bool isLowerAlphaNumeric(char c) noexcept {
            return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        }

        // Проверяет соответствие строки формату канонического ключа:
        //   - минимум 3 сегмента, разделённых '.';
        //   - первый символ каждого сегмента — строчная латинская буква;
        //   - остальные символы — строчные буквы, цифры, '_', '-';
        //   - нет trailing-точки.
        [[nodiscard]] bool isValidCanonicalKey(std::string_view key) noexcept {
            if (key.empty()) {
                return false;
            }

            std::size_t segmentCount = 0u;
            std::size_t i            = 0u;

            while (i < key.size()) {
                if (!isLowerAlpha(key[i])) {
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

                ++i; // пропускаем '.'
                if (i == key.size()) {
                    return false; // trailing-точка запрещена
                }
            }

            return segmentCount >= 3u;
        }

        void validateCanonicalKey(std::string_view key) {
            if (!isValidCanonicalKey(key)) {
                LOG_PANIC(core::log::cat::Resources,
                          "[ResourceRegistry] [RR-INVALID-KEY] "
                          "Недопустимый canonical key '{}'.",
                          key);
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
                LOG_PANIC(core::log::cat::Resources,
                          "[ResourceRegistry] Поле '{}' для '{}' должно быть булевым.",
                          fieldName,
                          entryName);
            }

            return it->get<bool>();
        }

        [[nodiscard]] std::string_view requireStringField(const Json& value,
                                                          std::string_view fieldName,
                                                          std::string_view entryName,
                                                          std::string_view typeName) {
            const auto it = value.find(fieldName);
            if (it == value.end()) {
                LOG_PANIC(core::log::cat::Resources,
                          "[ResourceRegistry] {} '{}': отсутствует обязательное поле '{}'.",
                          typeName,
                          entryName,
                          fieldName);
            }

            const auto* ptr = it->get_ptr<const std::string*>();
            if (ptr == nullptr) {
                LOG_PANIC(core::log::cat::Resources,
                          "[ResourceRegistry] {} '{}': поле '{}' должно быть строкой.",
                          typeName,
                          entryName,
                          fieldName);
            }

            if (ptr->empty()) {
                LOG_PANIC(core::log::cat::Resources,
                          "[ResourceRegistry] {} '{}': поле '{}' не должно быть пустым.",
                          typeName,
                          entryName,
                          fieldName);
            }

            return *ptr;
        }

        [[nodiscard]] std::optional<std::string_view>
            tryReadStringField(const Json& value, std::string_view fieldName) {
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
            std::string_view name{};
            std::string_view path{};
            Config           config{};
            StableKey        stableKey     = 0u;
            int              layerPriority = 0;
            int              loadOrder     = 0;
            std::string_view sourceName{};
        };

        template <typename Config>
        using DefinitionMap = std::unordered_map<std::string_view,
                                                  ResourceDefinition<Config>,
                                                  StringViewHash,
                                                  StringViewEqual>;

        void ensureUniqueInSource(SourceKeySet& sourceKeys, std::string_view key) {
            const auto [_, inserted] = sourceKeys.emplace(key);
            if (!inserted) {
                LOG_PANIC(core::log::cat::Resources,
                          "[ResourceRegistry] [RR-DUPLICATE-KEY-IN-SOURCE] "
                          "Дублирующийся canonical key '{}' в одном source-файле.",
                          key);
            }
        }

        template <typename Config>
        void insertOrOverride(DefinitionMap<Config>& map,
                              ResourceDefinition<Config> definition) {
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

            LOG_PANIC(
                core::log::cat::Resources,
                "[ResourceRegistry] [RR-OVERRIDE-TIE] "
                "Конфликт: canonical key '{}' с одинаковой политикой override: "
                "источники '{}' и '{}' имеют layerPriority={} и loadOrder={}.",
                definition.name,
                existing.sourceName,
                definition.sourceName,
                definition.layerPriority,
                definition.loadOrder);
        }

        void parseRegistryHeader(const Json& data, std::string_view path) {
            const auto it = data.find("version");
            if (it == data.end()) {
                LOG_PANIC(core::log::cat::Resources,
                          "[ResourceRegistry] Отсутствует 'version' в {}.",
                          path);
            }

            if (const auto* ptr = it->get_ptr<const Json::number_integer_t*>();
                ptr != nullptr) {
                if (*ptr != 1) {
                    LOG_PANIC(core::log::cat::Resources,
                              "[ResourceRegistry] Неподдерживаемая версия реестра {} в {}.",
                              *ptr,
                              path);
                }
                return;
            }

            if (const auto* ptr = it->get_ptr<const Json::number_unsigned_t*>();
                ptr != nullptr) {
                if (*ptr != 1u) {
                    LOG_PANIC(core::log::cat::Resources,
                              "[ResourceRegistry] Неподдерживаемая версия реестра {} в {}.",
                              *ptr,
                              path);
                }
                return;
            }

            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceRegistry] Некорректный тип 'version' в {}.",
                      path);
        }

        const Json& requireObjectBlock(const Json& data, std::string_view key) {
            const auto it = data.find(key);
            if (it == data.end()) {
                LOG_PANIC(core::log::cat::Resources,
                          "[ResourceRegistry] Отсутствует обязательный блок '{}'.",
                          key);
            }
            if (!it->is_object()) {
                LOG_PANIC(core::log::cat::Resources,
                          "[ResourceRegistry] Блок '{}' должен быть объектом.",
                          key);
            }
            return *it;
        }

        void parseTextureBlock(const Json& textures,
                               const ResourceSource& source,
                               registry::StringPool& strings,
                               SourceKeySet& sourceKeys,
                               DefinitionMap<config::TextureResourceConfig>& out) {
            for (auto it = textures.begin(); it != textures.end(); ++it) {
                const std::string_view key   = it.key();
                const Json&            value = it.value();

                validateCanonicalKey(key);
                ensureUniqueInSource(sourceKeys, key);

                if (!value.is_object()) {
                    LOG_PANIC(core::log::cat::Resources,
                              "[ResourceRegistry] Texture '{}' должна быть объектом.",
                              key);
                }

                const std::string_view path =
                    requireStringField(value, "path", key, "Texture");
#if !defined(NDEBUG)
                if (!FileLoader::fileExists(path)) {
                    LOG_PANIC(core::log::cat::Resources,
                              "[ResourceRegistry] Файл текстуры '{}' не найден для '{}'.",
                              path,
                              key);
                }
#endif
                config::TextureResourceConfig cfg{};
                cfg.smooth         = readBooleanWithDefault(value, "smooth",   true,  key);
                cfg.repeated       = readBooleanWithDefault(value, "repeated", false, key);
                cfg.generateMipmap = readBooleanWithDefault(value, "mipmap",   false, key);
                cfg.srgb           = readBooleanWithDefault(value, "srgb",     true,  key);

                ResourceDefinition<config::TextureResourceConfig> def{};
                def.name          = strings.intern(key);
                def.path          = strings.intern(path);
                def.config        = cfg;
                def.layerPriority = source.layerPriority;
                def.loadOrder     = source.loadOrder;
                def.sourceName    = source.sourceName;

                insertOrOverride(out, std::move(def));
            }
        }

        void parseFontBlock(const Json& fonts,
                            const ResourceSource& source,
                            registry::StringPool& strings,
                            SourceKeySet& sourceKeys,
                            DefinitionMap<config::FontResourceConfig>& out) {
            for (auto it = fonts.begin(); it != fonts.end(); ++it) {
                const std::string_view key   = it.key();
                const Json&            value = it.value();

                validateCanonicalKey(key);
                ensureUniqueInSource(sourceKeys, key);

                if (!value.is_object()) {
                    LOG_PANIC(core::log::cat::Resources,
                              "[ResourceRegistry] Font '{}' должен быть объектом.",
                              key);
                }

                const std::string_view path =
                    requireStringField(value, "path", key, "Font");
#if !defined(NDEBUG)
                if (!FileLoader::fileExists(path)) {
                    LOG_PANIC(core::log::cat::Resources,
                              "[ResourceRegistry] Файл шрифта '{}' не найден для '{}'.",
                              path,
                              key);
                }
#endif
                config::FontResourceConfig cfg{};

                ResourceDefinition<config::FontResourceConfig> def{};
                def.name          = strings.intern(key);
                def.path          = strings.intern(path);
                def.config        = cfg;
                def.layerPriority = source.layerPriority;
                def.loadOrder     = source.loadOrder;
                def.sourceName    = source.sourceName;

                insertOrOverride(out, std::move(def));
            }
        }

        void parseSoundBlock(const Json& sounds,
                             const ResourceSource& source,
                             registry::StringPool& strings,
                             SourceKeySet& sourceKeys,
                             DefinitionMap<config::SoundResourceConfig>& out) {
            for (auto it = sounds.begin(); it != sounds.end(); ++it) {
                const std::string_view key   = it.key();
                const Json&            value = it.value();

                validateCanonicalKey(key);
                ensureUniqueInSource(sourceKeys, key);

                if (!value.is_object()) {
                    LOG_WARN(core::log::cat::Resources,
                             "[ResourceRegistry] Sound '{}' должен быть объектом. Пропуск.",
                             key);
                    continue;
                }

                const auto pathOpt = tryReadStringField(value, "path");
                if (!pathOpt) {
                    LOG_WARN(core::log::cat::Resources,
                             "[ResourceRegistry] Sound '{}': отсутствует 'path'. Пропуск.",
                             key);
                    continue;
                }

                const std::string_view path = *pathOpt;
#if !defined(NDEBUG)
                if (!FileLoader::fileExists(path)) {
                    LOG_WARN(core::log::cat::Resources,
                             "[ResourceRegistry] Файл звука '{}' не найден для '{}'. Пропуск.",
                             path,
                             key);
                    continue;
                }
#endif
                config::SoundResourceConfig cfg{};

                ResourceDefinition<config::SoundResourceConfig> def{};
                def.name          = strings.intern(key);
                def.path          = strings.intern(path);
                def.config        = cfg;
                def.layerPriority = source.layerPriority;
                def.loadOrder     = source.loadOrder;
                def.sourceName    = source.sourceName;

                insertOrOverride(out, std::move(def));
            }
        }

        template <typename Config>
        void computeStableKeysAndValidateCollisions(DefinitionMap<Config>& defs,
                                                    StableKeyLookup& globalLookup) {
            for (auto& [_, def] : defs) {
                def.stableKey = computeStableKey64(def.name);

                auto [it, inserted] = globalLookup.emplace(def.stableKey, def.name);
                if (!inserted && it->second != def.name) {
                    LOG_PANIC(
                        core::log::cat::Resources,
                        "[ResourceRegistry] [RR-STABLEKEY-COLLISION] "
                        "Коллизия StableKey64: '{}' и '{}' имеют одинаковый хэш {}.",
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
            const std::size_t count    = definitions.size();
            const std::size_t maxCount = static_cast<std::size_t>(Key::MaxIndex) + 1u;
            if (count > maxCount) {
                LOG_PANIC(
                    core::log::cat::Resources,
                    "[ResourceRegistry] Превышено максимальное количество записей: {} (макс {}).",
                    count,
                    maxCount);
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
                entry.key       = Key::make(static_cast<std::uint32_t>(i));
                entry.stableKey = def.stableKey;
                entry.name      = def.name;
                entry.path      = def.path;
                entry.config    = def.config;

                entries.push_back(std::move(entry));
            }

            nameIndex.clear();
            nameIndex.reserve(entries.size());
            for (std::size_t i = 0; i < entries.size(); ++i) {
                nameIndex.push_back(
                    NameIndex{entries[i].name, static_cast<std::uint32_t>(i)});
            }

            std::sort(nameIndex.begin(),
                      nameIndex.end(),
                      [](const NameIndex& a, const NameIndex& b) {
                          return a.name < b.name;
                      });
        }

        template <typename Key, typename Entries>
        [[nodiscard]] Key findByNameIndex(const std::vector<NameIndex>& index,
                                          std::string_view name,
                                          const Entries& entries) {
            const auto it = std::lower_bound(
                index.begin(),
                index.end(),
                name,
                [](const NameIndex& e, std::string_view v) { return e.name < v; });

            if (it == index.end() || it->name != name) {
                return Key{};
            }

            const std::uint32_t idx = it->index;
            assert(idx < entries.size());
            return entries[idx].key;
        }

    } // namespace

    // --------------------------------------------------------------------------------------------
    // ResourceRegistry::loadFromSources
    // --------------------------------------------------------------------------------------------

    void ResourceRegistry::loadFromSources(std::span<const ResourceSource> sources) {
        mStrings = registry::StringPool{};

        mTextures.clear();
        mFonts.clear();
        mSounds.clear();
        mTextureNameIndex.clear();
        mFontNameIndex.clear();
        mSoundNameIndex.clear();

        mMissingTexture = TextureKey{};
        mMissingFont    = FontKey{};

        DefinitionMap<config::TextureResourceConfig> textureDefinitions;
        DefinitionMap<config::FontResourceConfig>    fontDefinitions;
        DefinitionMap<config::SoundResourceConfig>   soundDefinitions;

        for (const auto& source : sources) {
            const auto content = FileLoader::loadTextFile(source.path);
            if (!content) {
                LOG_PANIC(core::log::cat::Resources,
                          "[ResourceRegistry] Не удалось загрузить файл реестра '{}'.",
                          source.path);
            }

            const Json data = core::utils::json::parseAndValidateCritical(
                *content,
                source.path,
                kRegistryModule,
                {JsonValidator::KeyRule{"version",
                                        {Json::value_t::number_integer,
                                         Json::value_t::number_unsigned},
                                        true},
                 JsonValidator::KeyRule{"textures", {Json::value_t::object}, true},
                 JsonValidator::KeyRule{"fonts",    {Json::value_t::object}, true},
                 JsonValidator::KeyRule{"sounds",   {Json::value_t::object}, true}});

            parseRegistryHeader(data, source.path);

            const Json& textures = requireObjectBlock(data, "textures");
            const Json& fonts    = requireObjectBlock(data, "fonts");
            const Json& sounds   = requireObjectBlock(data, "sounds");

            SourceKeySet sourceKeys;
            sourceKeys.reserve(textures.size() + fonts.size() + sounds.size());

            parseTextureBlock(textures, source, mStrings, sourceKeys, textureDefinitions);
            parseFontBlock(fonts,   source, mStrings, sourceKeys, fontDefinitions);
            parseSoundBlock(sounds, source, mStrings, sourceKeys, soundDefinitions);
        }

        const std::size_t totalDefs =
            textureDefinitions.size() + fontDefinitions.size() + soundDefinitions.size();

        StableKeyLookup stableKeyGlobal;
        stableKeyGlobal.reserve(totalDefs);

        computeStableKeysAndValidateCollisions(textureDefinitions, stableKeyGlobal);
        computeStableKeysAndValidateCollisions(fontDefinitions,    stableKeyGlobal);
        computeStableKeysAndValidateCollisions(soundDefinitions,   stableKeyGlobal);

        finalizeEntries<TextureEntry, config::TextureResourceConfig, TextureKey>(
            textureDefinitions, mTextures, mTextureNameIndex);

        finalizeEntries<FontEntry, config::FontResourceConfig, FontKey>(
            fontDefinitions, mFonts, mFontNameIndex);

        finalizeEntries<SoundEntry, config::SoundResourceConfig, SoundKey>(
            soundDefinitions, mSounds, mSoundNameIndex);

        mMissingTexture = findTextureByName(kMissingTextureName);
        if (!mMissingTexture.valid()) {
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceRegistry] [RR-MISSING-FALLBACK-TEXTURE] "
                      "Отсутствует обязательная fallback-текстура '{}'.",
                      kMissingTextureName);
        }

        mMissingFont = findFontByName(kMissingFontName);
        if (!mMissingFont.valid()) {
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceRegistry] [RR-MISSING-FALLBACK-FONT] "
                      "Отсутствует обязательный fallback-шрифт '{}'.",
                      kMissingFontName);
        }

        mStrings.clearLookup();
    }

    // --------------------------------------------------------------------------------------------
    // Runtime API
    // --------------------------------------------------------------------------------------------

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

    // --------------------------------------------------------------------------------------------
    // API отладки / инструментов — O(log N)
    // --------------------------------------------------------------------------------------------

    TextureKey ResourceRegistry::findTextureByName(std::string_view name) const {
        return findByNameIndex<TextureKey>(mTextureNameIndex, name, mTextures);
    }

    TextureKey ResourceRegistry::findTextureByStableKey(
        const std::uint64_t stableKey) const {
        const auto it = std::lower_bound(
            mTextures.begin(),
            mTextures.end(),
            stableKey,
            [](const TextureEntry& entry, std::uint64_t value) {
                return entry.stableKey < value;
            });

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

    // --------------------------------------------------------------------------------------------
    // Fallback-ключи
    // --------------------------------------------------------------------------------------------

    TextureKey ResourceRegistry::missingTextureKey() const noexcept {
        return mMissingTexture;
    }

    FontKey ResourceRegistry::missingFontKey() const noexcept {
        return mMissingFont;
    }

    // --------------------------------------------------------------------------------------------
    // Метрики
    // --------------------------------------------------------------------------------------------

    std::size_t ResourceRegistry::textureCount() const noexcept {
        return mTextures.size();
    }

    std::size_t ResourceRegistry::fontCount() const noexcept {
        return mFonts.size();
    }

    std::size_t ResourceRegistry::soundCount() const noexcept {
        return mSounds.size();
    }

} // namespace core::resources

#if defined(_MSC_VER)
    #pragma warning(pop)
#endif