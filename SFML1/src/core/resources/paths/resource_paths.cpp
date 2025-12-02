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

    namespace registry_keys {
        constexpr const char* Textures = "textures";
        constexpr const char* Fonts = "fonts";
        constexpr const char* Sounds = "sounds";
    } // namespace registry_keys

    // --------------------------------------------------------------------------------------------
    // Универсальный загрузчик карты ID -> путь (DRY principle).
    // Поддерживает:
    //  - разные enum'ы (TextureID / FontID / SoundID),
    //  - разные ключи верхнего уровня в JSON ("textures", "fonts", "sounds"),
    //  - разные функции парсинга строкового ID -> enum (textureFromString, ...).
    // --------------------------------------------------------------------------------------------
    template <typename EnumID, typename Mapper>
    void loadResourceMap(const Json& data, const char* keyName, Mapper mapper,
                         std::unordered_map<EnumID, std::string>& outMap) {

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

            outMap[*idOpt] = value.get<std::string>();
        }
    }

    // --------------------------------------------------------------------------------------------
    // Универсальный геттер с проверкой (DRY principle).
    // Бросает std::runtime_error, если путь не найден.
    // --------------------------------------------------------------------------------------------
    template <typename EnumID>
        const std::string& getResourcePath(const std::unordered_map<EnumID, std::string>& map,
                                           EnumID id, const char* typeName) {

        const auto it = map.find(id);
        if (it == map.end()) {
            throw std::runtime_error(std::string("[ResourcePaths::get(") + typeName +
                                     ")]\nНе найден путь для " + typeName + ": " +
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

    std::unordered_map<ids::TextureID, std::string>& ResourcePaths::getTextureMap() {
        static std::unordered_map<ids::TextureID, std::string> instance;
        return instance;
    }

    std::unordered_map<ids::FontID, std::string>& ResourcePaths::getFontMap() {
        static std::unordered_map<ids::FontID, std::string> instance;
        return instance;
    }

    std::unordered_map<ids::SoundID, std::string>& ResourcePaths::getSoundMap() {
        static std::unordered_map<ids::SoundID, std::string> instance;
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

        // Заполнение карт через универсальный шаблон
        loadResourceMap(data, registry_keys::Textures, ids::textureFromString, getTextureMap());
        loadResourceMap(data, registry_keys::Fonts, ids::fontFromString, getFontMap());
        loadResourceMap(data, registry_keys::Sounds, ids::soundFromString, getSoundMap());
    }

    // ============================================================================================
    // Геттеры
    // ============================================================================================

    const std::string& ResourcePaths::get(ids::TextureID id) {
        return getResourcePath(getTextureMap(), id, "TextureID");
    }

    const std::string& ResourcePaths::get(ids::FontID id) {
        return getResourcePath(getFontMap(), id, "FontID");
    }

    const std::string& ResourcePaths::get(ids::SoundID id) {
        return getResourcePath(getSoundMap(), id, "SoundID");
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

    const std::unordered_map<ids::TextureID, std::string>&
    ResourcePaths::getAllTexturePaths() noexcept {
        return getTextureMap();
    }

    const std::unordered_map<ids::FontID, std::string>& ResourcePaths::getAllFontPaths() noexcept {
        return getFontMap();
    }

    const std::unordered_map<ids::SoundID, std::string>&
    ResourcePaths::getAllSoundPaths() noexcept {
        return getSoundMap();
    }

} // namespace core::resources::paths