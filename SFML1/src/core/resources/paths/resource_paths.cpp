#include "pch.h"

#include "core/resources/paths/resource_paths.h"
#include <cstdlib>
#include <stdexcept>

#include "core/resources/ids/resource_id_utils.h"
#include "core/utils/file_loader.h"
#include "core/utils/json/json_utils.h"
#include "core/utils/json/json_validator.h"
#include "core/utils/message.h"

namespace {

    using core::utils::FileLoader;
    namespace message = core::utils::message;
    namespace json_utils = core::utils::json;

    using Json = json_utils::json;
    using JsonValidator = json_utils::JsonValidator;

    using TextureConfig = core::resources::paths::ResourcePaths::TextureConfig;
    using FontConfig = core::resources::paths::ResourcePaths::FontConfig;
    using SoundConfig = core::resources::paths::ResourcePaths::SoundConfig;

    namespace registry_keys {
        constexpr const char* Textures = "textures";
        constexpr const char* Fonts = "fonts";
        constexpr const char* Sounds = "sounds";
    } // namespace registry_keys

    // --------------------------------------------------------------------------------------------
    // Универсальный загрузчик простых карт ID -> Config(path), где value = строка пути.
    // Используется для FontConfig / SoundConfig.
    // --------------------------------------------------------------------------------------------
    template <typename EnumID, typename Mapper, typename Config>
    void loadSimplePathConfigMap(const Json& data, const char* keyName, Mapper mapper,
                                 std::unordered_map<EnumID, Config>& outMap) {

        outMap.clear();

        // Ключ может отсутствовать (например, звуки ещё не используются).
        if (!data.contains(keyName)) {
            return;
        }

        const Json& node = data.at(keyName);
        if (!node.is_object()) {
            message::logDebug("[ResourcePaths]\n" + std::string(keyName) +
                              " не является объектом, пропускаем.");
            return;
        }

        outMap.reserve(node.size());

        for (auto it = node.begin(); it != node.end(); ++it) {
            const std::string& idStr = it.key();
            const Json& value = it.value();

            if (!value.is_string()) {
                message::logDebug("[ResourcePaths]\nПропущен " + std::string(keyName) + " '" +
                                  idStr + "': значение должно быть строкой пути.");
                continue;
            }

            auto idOpt = mapper(idStr);
            if (!idOpt) {
                message::logDebug("[ResourcePaths]\nНеизвестный ID в resources.json: " + idStr);
                continue;
            }

            Config cfg;
            cfg.path = value.get<std::string>();
            outMap[*idOpt] = std::move(cfg);
        }
    }

    // --------------------------------------------------------------------------------------------
    // Загрузчик карты TextureID -> TextureConfig.
    //
    // Поддерживает один канонический формат JSON:
    //
    //  "textures": {
    //      "Player": {
    //          "path": "assets/game/skyguard/images/0000su-57.png",
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
        const Json& data, const char* keyName, Mapper mapper,
        std::unordered_map<core::resources::ids::TextureID, TextureConfig>& outMap) {

        using core::resources::ids::TextureID;

        outMap.clear();

        if (!data.contains(keyName)) {
            return;
        }

        const Json& node = data.at(keyName);
        if (!node.is_object()) {
            message::logDebug("[ResourcePaths]\n" + std::string(keyName) +
                              " не является объектом, пропускаем.");
            return;
        }

        outMap.reserve(node.size());

        for (auto it = node.begin(); it != node.end(); ++it) {
            const std::string& idString = it.key();
            const Json& value = it.value();

            auto idOpt = mapper(idString);
            if (!idOpt) {
                message::logDebug("[ResourcePaths]\nНеизвестный TextureID в resources.json: " +
                                  idString);
                continue;
            }

            if (!value.is_object()) {
                message::logDebug("[ResourcePaths]\nПропущена текстура '" + idString +
                                  "': ожидался объект с полями конфигурации.");
                continue;
            }

            const Json& object = value;
            TextureConfig cfg;

            // path — обязателен.
            const auto pathIterator = object.find("path");
            if (pathIterator == object.end() || !pathIterator->is_string()) {
                message::logDebug("[ResourcePaths]\nПропущена текстура '" + idString +
                                  "': поле 'path' обязательно и должно быть строкой.");
                continue;
            }

            cfg.path = pathIterator->get<std::string>();

            // Вспомогательная лямбда для булевых флагов.
            auto readBooleanWithDefault = [&](const char* fieldName, bool defaultValue) {
                const auto flagIterator = object.find(fieldName);
                if (flagIterator == object.end()) {
                    return defaultValue;
                }
                if (!flagIterator->is_boolean()) {
                    message::logDebug("[ResourcePaths]\nИгнорируем поле '" +
                                      std::string(fieldName) + "' для текстуры '" + idString +
                                      "': ожидался boolean. Используем значение по умолчанию.");
                    return defaultValue;
                }
                return flagIterator->get<bool>();
            };

            cfg.smooth = readBooleanWithDefault("smooth", true);
            cfg.repeated = readBooleanWithDefault("repeated", false);
            cfg.generateMipmap = readBooleanWithDefault("mipmap", false);

            outMap[*idOpt] = std::move(cfg);
        }
    }

    // --------------------------------------------------------------------------------------------
    // Универсальный геттер с проверкой (DRY principle).
    // Бросает std::runtime_error, если конфиг не найден.
    // --------------------------------------------------------------------------------------------
    template <typename EnumID, typename Config>
    const Config& getResourceConfig(const std::unordered_map<EnumID, Config>& map, EnumID id,
                                    const char* typeName) {

        const auto it = map.find(id);
        if (it == map.end()) {
            throw std::runtime_error(std::string("[ResourcePaths::get") + typeName +
                                     "]\nНе найден ресурс для " + typeName + ": " +
                                     std::string(core::resources::ids::toString(id)));
        }
        return it->second;
    }

    // --------------------------------------------------------------------------------------------
    // Валидация JSON-структуры базового реестра ресурсов.
    // --------------------------------------------------------------------------------------------
    void validateResourceRegistry(const Json& data) {
        JsonValidator::validate(data, {
                                          {registry_keys::Textures, {Json::value_t::object}, true},
                                          {registry_keys::Fonts, {Json::value_t::object}, true},
                                          {registry_keys::Sounds, {Json::value_t::object}, false},
                                      });
    }

} // namespace

namespace core::resources::paths {

    // ============================================================================================
    // Meyer's Singleton pattern для внутренних map'ов (thread-safe initialization).
    // ============================================================================================

    std::unordered_map<ids::TextureID, ResourcePaths::TextureConfig>&
    ResourcePaths::getTextureMap() {
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

    // ============================================================================================
    // Загрузка реестра из JSON
    // ============================================================================================

    void ResourcePaths::loadFromJSON(const std::string& filename) {
        // Чтение файла
        const auto fileContentOpt = FileLoader::loadTextFile(filename);
        if (!fileContentOpt) {
            message::showError("[ResourcePaths]\nНе удалось открыть реестр ресурсов: " + filename);
            std::exit(EXIT_FAILURE);
        }

        // Парсинг JSON
        Json data;
        try {
            data = Json::parse(*fileContentOpt);
        } catch (const std::exception& e) {
            message::showError("[ResourcePaths]\nОшибка парсинга JSON в " + filename + ": " +
                               e.what());
            std::exit(EXIT_FAILURE);
        } catch (...) {
            message::showError("[ResourcePaths]\nНеизвестная ошибка при парсинге JSON: " +
                               filename);
            std::exit(EXIT_FAILURE);
        }

        // Валидация структуры
        try {
            validateResourceRegistry(data);
        } catch (const std::exception& e) {
            message::showError("[ResourcePaths]\nНеверная структура JSON: " +
                               std::string(e.what()));
            std::exit(EXIT_FAILURE);
        }

        // Заполнение карт через специализированные загрузчики.
        loadTextureConfigMap(data, registry_keys::Textures, ids::textureFromString,
                             getTextureMap());
        loadSimplePathConfigMap(data, registry_keys::Fonts, ids::fontFromString, getFontMap());
        loadSimplePathConfigMap(data, registry_keys::Sounds, ids::soundFromString, getSoundMap());
    }

    // ============================================================================================
    // Геттеры конфигураций
    // ============================================================================================

    const ResourcePaths::TextureConfig& ResourcePaths::getTextureConfig(ids::TextureID id) {
        return getResourceConfig(getTextureMap(), id, "(TextureID)");
    }

    const ResourcePaths::FontConfig& ResourcePaths::getFontConfig(ids::FontID id) {
        return getResourceConfig(getFontMap(), id, "(FontID)");
    }

    const ResourcePaths::SoundConfig& ResourcePaths::getSoundConfig(ids::SoundID id) {
        return getResourceConfig(getSoundMap(), id, "(SoundID)");
    }

    // ============================================================================================
    // Contains-методы
    // ============================================================================================

    bool ResourcePaths::contains(ids::TextureID id) noexcept {
        return getTextureMap().find(id) != getTextureMap().end();
    }

    bool ResourcePaths::contains(ids::FontID id) noexcept {
        return getFontMap().find(id) != getFontMap().end();
    }

    bool ResourcePaths::contains(ids::SoundID id) noexcept {
        return getSoundMap().find(id) != getSoundMap().end();
    }

    // ============================================================================================
    // Геттеры доступа ко всей коллекции сразу (для пакетной загрузки / предзагрузки)
    // ============================================================================================

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