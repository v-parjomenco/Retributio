#include "pch.h"

#include "core/resources/paths/resource_paths.h"

#include <cassert>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "core/log/log_macros.h"
#include "core/resources/ids/resource_id_utils.h"
#include "core/utils/file_loader.h"
#include "core/utils/json/json_document.h"

namespace {

    using core::utils::FileLoader;
    namespace json_utils = core::utils::json;

    using Json = json_utils::json;

    using TextureConfig = core::resources::paths::ResourcePaths::TextureConfig;
    using FontConfig    = core::resources::paths::ResourcePaths::FontConfig;
    using SoundConfig   = core::resources::paths::ResourcePaths::SoundConfig;

    namespace registry_keys {
        constexpr const char* Textures = "textures";
        constexpr const char* Fonts    = "fonts";
        constexpr const char* Sounds   = "sounds";
    } // namespace registry_keys

    // --------------------------------------------------------------------------------------------
    // Универсальный загрузчик простых мап ID -> Config(path), где value = строка пути.
    // Strict mode: любая ошибка -> LOG_PANIC.
    // --------------------------------------------------------------------------------------------
    template <typename EnumID, typename Mapper, typename Config>
    void loadSimplePathConfigMapStrict(const Json& data,
                                       std::string_view keyName,
                                       Mapper mapper,
                                       std::unordered_map<EnumID, Config>& outMap) {

        outMap.clear();

        const auto it = data.find(keyName);
        if (it == data.end()) {
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourcePaths] Отсутствует обязательный блок '{}'.",
                      keyName);
        }

        const Json& node = *it;
        if (!node.is_object()) {
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourcePaths] Блок '{}' должен быть объектом.",
                      keyName);
        }

        outMap.reserve(node.size());

        for (auto nodeIt = node.begin(); nodeIt != node.end(); ++nodeIt) {
            std::string_view idStr = nodeIt.key();
            const Json&      value = nodeIt.value();

            if (!value.is_string()) {
                LOG_PANIC(core::log::cat::Resources,
                          "[ResourcePaths] Некорректный {} '{}': "
                          "значение должно быть строкой пути.",
                          keyName,
                          idStr);
            }

            // Структурный if-else: делаем flow explicit для статического анализатора.
            // idOpt валиден только в else-ветке, где гарантированно mapper вернул значение.
            if (const auto idOpt = mapper(idStr); !idOpt) {
                LOG_PANIC(core::log::cat::Resources,
                          "[ResourcePaths] Неизвестный ID в resources.json: {}",
                          idStr);
            } else {
                Config cfg{};
                cfg.path = value.get<std::string>();

                // emplace вместо operator[]: избегаем default-construct + move-assign.
                // Дубликатов ключей быть не должно (валидируем на этапе authoring/парсинга).
                outMap.emplace(*idOpt, std::move(cfg));
            }
        }
    }

    // --------------------------------------------------------------------------------------------
    // Мягкий режим загрузчика звуков: неизвестные ID или недопустимые записи -> LOG_WARN + пропуск.
    // Допускается отсутствие блока.
    // --------------------------------------------------------------------------------------------
    template <typename EnumID, typename Mapper, typename Config>
    void loadSimplePathConfigMapSoft(const Json& data,
                                     std::string_view keyName,
                                     Mapper mapper,
                                     std::unordered_map<EnumID, Config>& outMap) {

        outMap.clear();

        const auto it = data.find(keyName);
        if (it == data.end()) {
            // sounds: блок опциональный. Отсутствие = нормальный сценарий (ничего не грузим).
            return;
        }

        const Json& node = *it;
        if (!node.is_object()) {
            LOG_WARN(core::log::cat::Resources,
                     "[ResourcePaths] Блок '{}' не является объектом, пропускаем весь блок.",
                     keyName);
            return;
        }

        outMap.reserve(node.size());

        for (auto nodeIt = node.begin(); nodeIt != node.end(); ++nodeIt) {
            std::string_view idStr = nodeIt.key();
            const Json&      value = nodeIt.value();

            if (!value.is_string()) {
                LOG_WARN(core::log::cat::Resources,
                         "[ResourcePaths] Пропущен {} '{}': значение должно быть строкой пути.",
                         keyName,
                         idStr);
                continue;
            }

            if (const auto idOpt = mapper(idStr); !idOpt) {
                LOG_WARN(core::log::cat::Resources,
                         "[ResourcePaths] Неизвестный ID в resources.json: {}",
                         idStr);
            } else {
                Config cfg{};
                cfg.path = value.get<std::string>();

                // emplace вместо operator[]: избегаем default-construct + move-assign.
                outMap.emplace(*idOpt, std::move(cfg));
            }
        }
    }

    // --------------------------------------------------------------------------------------------
    // Загрузчик карты TextureID -> TextureConfig.
    //
    // Поддерживает один канонический формат JSON:
    //
    //  "textures": {
    //      "Player": {
    //          "path": "assets/game/skyguard/images/su-57.png",
    //          "smooth": true,
    //          "repeated": false,
    //          "mipmap": false
    //      }
    //  }
    //
    // Поле "mipmap" в JSON мапится на флаг TextureResourceConfig::generateMipmap.
    // --------------------------------------------------------------------------------------------
    template <typename Mapper>
    void loadTextureConfigMap(
        const Json& data,
        std::string_view keyName,
        Mapper mapper,
        std::unordered_map<core::resources::ids::TextureID, TextureConfig>& outMap) {

        outMap.clear();

        const auto it = data.find(keyName);
        if (it == data.end()) {
            return;
        }

        const Json& node = *it;
        if (!node.is_object()) {
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourcePaths] Блок '{}' должен быть объектом.",
                      keyName);
        }

        outMap.reserve(node.size());

        for (auto nodeIt = node.begin(); nodeIt != node.end(); ++nodeIt) {
            std::string_view idString = nodeIt.key();
            const Json&      value    = nodeIt.value();

            // Структурный if-else: проверяем ID первым, весь happy path идёт в else-блок.
            // Это устраняет C26829 (MSVC Intellisense не может протрассировать [[noreturn]]
            // через макрос LOG_PANIC, но понимает, что в else-ветке idOpt гарантированно валиден).
            if (const auto idOpt = mapper(idString); !idOpt) {
                LOG_PANIC(core::log::cat::Resources,
                          "[ResourcePaths] Неизвестный TextureID в resources.json: {}",
                          idString);
            } else {
                if (!value.is_object()) {
                    LOG_PANIC(core::log::cat::Resources,
                              "[ResourcePaths] Некорректная текстура '{}': ожидался объект с "
                              "полями конфигурации.",
                              idString);
                }

                const auto id = *idOpt;

                TextureConfig cfg{};

                // path — обязателен.
                const auto pathIterator = value.find("path");
                if (pathIterator == value.end() || !pathIterator->is_string()) {
                    LOG_PANIC(core::log::cat::Resources,
                              "[ResourcePaths] Некорректная текстура '{}': поле 'path' обязательно "
                              "и должно быть строкой.",
                              idString);
                }

                cfg.path = pathIterator->get<std::string>();

                // Вспомогательная лямбда для булевых флагов.
                auto readBooleanWithDefault = [&](const char* fieldName, bool defaultValue) {
                    const auto flagIterator = value.find(fieldName);
                    if (flagIterator == value.end()) {
                        return defaultValue;
                    }
                    if (!flagIterator->is_boolean()) {
                        LOG_PANIC(core::log::cat::Resources,
                                  "[ResourcePaths] Некорректное поле '{}' для текстуры '{}': "
                                  "ожидался boolean.",
                                  fieldName,
                                  idString);
                    }
                    return flagIterator->get<bool>();
                };

                cfg.smooth         = readBooleanWithDefault("smooth", true);
                cfg.repeated       = readBooleanWithDefault("repeated", false);
                cfg.generateMipmap = readBooleanWithDefault("mipmap", false);

                // Здесь operator[] ок: по контракту ключа ещё нет, cfg уже полностью готов.
                outMap[id] = std::move(cfg);
            }
        }
    }

    // --------------------------------------------------------------------------------------------
    // Универсальный геттер с проверкой (DRY).
    // Контракт: ID обязан существовать; иначе Debug: assert, Release/Profile: LOG_PANIC.
    // --------------------------------------------------------------------------------------------
    template <typename EnumID, typename Config>
    const Config& getResourceConfig(const std::unordered_map<EnumID, Config>& map,
                                    EnumID id,
                                    const char* typeName) {

        const auto it = map.find(id);

        // Debug: ловим программистскую ошибку максимально рано и дёшево.
        assert(it != map.end() && "Resource ID must be registered in resources.json");

        if (it == map.end()) {
            // Release/Profile: даём полный контекст и падаем согласно политике.
            using U = std::underlying_type_t<EnumID>;
            const auto raw = static_cast<std::uint64_t>(static_cast<U>(id));

            LOG_PANIC(core::log::cat::Resources,
                      "[ResourcePaths::getResourceConfig{}] Не найден ресурс: id={}. "
                      "ID не зарегистрирован в resources.json или "
                      "вызов произошёл до loadFromJSON().",
                      typeName,
                      raw);
        }

        return it->second;
    }

} // namespace

namespace core::resources::paths {

    // --------------------------------------------------------------------------------------------
    // Meyer's Singleton pattern для внутренних map'ов (thread-safe initialization).
    // --------------------------------------------------------------------------------------------

    std::unordered_map<ids::TextureID, ResourcePaths::TextureConfig>&
    ResourcePaths::getTextureMap() {
        // ВАЖНО:
        //  - Meyer's Singleton инициализируется при первом вызове (C++11+ thread-safe).
        //  - Живёт до завершения процесса (static storage duration).
        //  - Не вызывать из деструкторов глобальных/статических объектов!
        static std::unordered_map<ids::TextureID, TextureConfig> instance;
        return instance;
    }

    std::unordered_map<ids::FontID, ResourcePaths::FontConfig>& ResourcePaths::getFontMap() {
        static std::unordered_map<ids::FontID, FontConfig> instance;
        return instance;
    }

    std::unordered_map<ids::SoundID, ResourcePaths::SoundConfig>& ResourcePaths::getSoundMap() {
        static std::unordered_map<ids::SoundID, SoundConfig> instance;
        return instance;
    }

    // --------------------------------------------------------------------------------------------
    // Загрузка реестра из JSON
    // --------------------------------------------------------------------------------------------

    void ResourcePaths::loadFromJSON(const std::string& filename) {
#ifndef NDEBUG
        // Контракт: loadFromJSON вызывается ровно один раз при старте.
        // Повторная загрузка = programmer error (возможна инвалидация ссылок/итераторов).
        static bool sLoadedOnce = false;
        assert(!sLoadedOnce &&
               "ResourcePaths::loadFromJSON must be called exactly once during startup");
        sLoadedOnce = true;
#endif

        // Чтение файла.
        const auto fileContentOpt = FileLoader::loadTextFile(filename);
        if (!fileContentOpt) {
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourcePaths] Не удалось открыть реестр ресурсов: {}",
                      filename);
        }

        // Парсинг и валидация через общий helper.
        const Json data = json_utils::parseAndValidateCritical(
            *fileContentOpt,
            filename,
            "ResourcePaths",
            {
                {registry_keys::Textures, {Json::value_t::object}, true},
                {registry_keys::Fonts,    {Json::value_t::object}, true},

                // sounds: soft-политика. Поле опционально, а при неверном типе мы НЕ падаем,
                // потому что хотим LOG_WARN + skip внутри loadSimplePathConfigMapSoft(...).
                // Поэтому разрешаем любой JSON-тип на этом уровне
                // (валидация типа — в soft-лоадере).
                {registry_keys::Sounds,
                 {
                     Json::value_t::null,
                     Json::value_t::object,
                     Json::value_t::array,
                     Json::value_t::string,
                     Json::value_t::boolean,
                     Json::value_t::number_integer,
                     Json::value_t::number_unsigned,
                     Json::value_t::number_float
                 },
                 false},
            });

        // Заполнение карт через специализированные загрузчики.
        loadTextureConfigMap(data,
                             registry_keys::Textures,
                             ids::fromString<ids::TextureID>,
                             getTextureMap());
        loadSimplePathConfigMapStrict(data,
                                      registry_keys::Fonts,
                                      ids::fromString<ids::FontID>,
                                      getFontMap());
        loadSimplePathConfigMapSoft(data,
                                    registry_keys::Sounds,
                                    ids::fromString<ids::SoundID>,
                                    getSoundMap());
    }

    // --------------------------------------------------------------------------------------------
    // Геттеры конфигураций
    // --------------------------------------------------------------------------------------------

    const ResourcePaths::TextureConfig& ResourcePaths::getTextureConfig(ids::TextureID id) {
        return getResourceConfig(getTextureMap(), id, "(TextureID)");
    }

    const ResourcePaths::FontConfig& ResourcePaths::getFontConfig(ids::FontID id) {
        return getResourceConfig(getFontMap(), id, "(FontID)");
    }

    const ResourcePaths::SoundConfig& ResourcePaths::getSoundConfig(ids::SoundID id) {
        return getResourceConfig(getSoundMap(), id, "(SoundID)");
    }

    // --------------------------------------------------------------------------------------------
    // Contains-методы
    // --------------------------------------------------------------------------------------------

    bool ResourcePaths::contains(ids::TextureID id) noexcept {
        const auto& map = getTextureMap();
        return map.find(id) != map.end();
    }

    bool ResourcePaths::contains(ids::FontID id) noexcept {
        const auto& map = getFontMap();
        return map.find(id) != map.end();
    }

    bool ResourcePaths::contains(ids::SoundID id) noexcept {
        const auto& map = getSoundMap();
        return map.find(id) != map.end();
    }

    // --------------------------------------------------------------------------------------------
    // Геттеры доступа ко всей коллекции сразу (для пакетной загрузки / предзагрузки)
    // --------------------------------------------------------------------------------------------

    const std::unordered_map<ids::TextureID, ResourcePaths::TextureConfig>&
    ResourcePaths::getAllTextureConfigs() noexcept {
        return getTextureMap();
    }

    const std::unordered_map<ids::FontID, ResourcePaths::FontConfig>&
    ResourcePaths::getAllFontConfigs() noexcept {
        return getFontMap();
    }

    const std::unordered_map<ids::SoundID, ResourcePaths::SoundConfig>&
    ResourcePaths::getAllSoundConfigs() noexcept {
        return getSoundMap();
    }

} // namespace core::resources::paths