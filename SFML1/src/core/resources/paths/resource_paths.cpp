#include "pch.h"

#include "core/resources/paths/resource_paths.h"
#include <cstdlib>   // для std::exit, EXIT_FAILURE
#include <stdexcept> // для std::runtime_error

#include "core/config/config_keys.h"
#include "core/resources/ids/id_to_string.h"
#include "core/utils/file_loader.h"
#include "core/utils/json/json_utils.h"
#include "core/utils/json/json_validator.h"
#include "core/utils/message.h"

namespace {

    using core::utils::FileLoader;

    namespace message = core::utils::message;
    namespace keys = core::config::keys;
    namespace json_utils = core::utils::json;

    using Json = json_utils::json;
    using JsonValidator = json_utils::JsonValidator;

    using core::resources::ids::fontFromString;
    using core::resources::ids::FontID;
    using core::resources::ids::soundFromString;
    using core::resources::ids::SoundID;
    using core::resources::ids::textureFromString;
    using core::resources::ids::TextureID;
    using core::resources::ids::toString;

    // --------------------------------------------------------------------------------------------
    // Валидация верхнего уровня JSON-структуры resources.json
    // --------------------------------------------------------------------------------------------

    void validateResourceRegistry(const Json& data) {
        using VR = JsonValidator::KeyRule;

        // textures и fonts — обязательные объекты, но могут быть пустыми.
        // sounds — необязателен (может пока не использоваться).
        JsonValidator::validate(
            data, {
                      {keys::ResourceRegistry::TEXTURES, {Json::value_t::object}, true},
                      {keys::ResourceRegistry::FONTS, {Json::value_t::object}, true},
                      {keys::ResourceRegistry::SOUNDS, {Json::value_t::object}, false},
                  });
    }

    // --------------------------------------------------------------------------------------------
    // Заполнение внутренних map'ов из JSON-объектов
    // --------------------------------------------------------------------------------------------

    void loadTextureMap(const Json& data, std::unordered_map<TextureID, std::string>& outTextures) {
        outTextures.clear();

        if (!data.contains(keys::ResourceRegistry::TEXTURES)) {
            return;
        }

        const Json& node = data.at(keys::ResourceRegistry::TEXTURES);
        if (!node.is_object()) {
            // validateResourceRegistry уже должен был это отсеять, но на всякий случай проверим.
            message::logDebug("[ResourcePaths]\ntextures не является объектом, пропускаем.");
            return;
        }

        for (auto it = node.begin(); it != node.end(); ++it) {
            const std::string& idStr = it.key();
            const Json& value = it.value();

            if (!value.is_string()) {
                message::logDebug("[ResourcePaths]\nПропущен texture '" + idStr +
                                  "': значение должно быть строкой пути.");
                continue;
            }

            auto idOpt = textureFromString(idStr);
            if (!idOpt) {
                message::logDebug("[ResourcePaths]\nНеизвестный TextureID в resources.json: " +
                                  idStr);
                continue;
            }

            outTextures[*idOpt] = value.get<std::string>();
        }
    }

    void loadFontMap(const Json& data, std::unordered_map<FontID, std::string>& outFonts) {
        outFonts.clear();

        if (!data.contains(keys::ResourceRegistry::FONTS)) {
            return;
        }

        const Json& node = data.at(keys::ResourceRegistry::FONTS);
        if (!node.is_object()) {
            message::logDebug("[ResourcePaths]\nfonts не является объектом, пропускаем.");
            return;
        }

        for (auto it = node.begin(); it != node.end(); ++it) {
            const std::string& idStr = it.key();
            const Json& value = it.value();

            if (!value.is_string()) {
                message::logDebug("[ResourcePaths]\nПропущен font '" + idStr +
                                  "': значение должно быть строкой пути.");
                continue;
            }

            auto idOpt = fontFromString(idStr);
            if (!idOpt) {
                message::logDebug("[ResourcePaths]\nНеизвестный FontID в resources.json: " + idStr);
                continue;
            }

            outFonts[*idOpt] = value.get<std::string>();
        }
    }

    void loadSoundMap(const Json& data, std::unordered_map<SoundID, std::string>& outSounds) {
        outSounds.clear();

        if (!data.contains(keys::ResourceRegistry::SOUNDS)) {
            return;
        }

        const Json& node = data.at(keys::ResourceRegistry::SOUNDS);
        if (!node.is_object()) {
            message::logDebug("[ResourcePaths]\nsounds не является объектом, пропускаем.");
            return;
        }

        for (auto it = node.begin(); it != node.end(); ++it) {
            const std::string& idStr = it.key();
            const Json& value = it.value();

            if (!value.is_string()) {
                message::logDebug("[ResourcePaths]\nПропущен sound '" + idStr +
                                  "': значение должно быть строкой пути.");
                continue;
            }

            auto idOpt = soundFromString(idStr);
            if (!idOpt) {
                message::logDebug("[ResourcePaths]\nНеизвестный SoundID в resources.json: " +
                                  idStr);
                continue;
            }

            outSounds[*idOpt] = value.get<std::string>();
        }
    }

} // namespace

namespace core::resources::paths {

    std::unordered_map<ids::TextureID, std::string> ResourcePaths::mTextures;
    std::unordered_map<ids::FontID, std::string> ResourcePaths::mFonts;
    std::unordered_map<ids::SoundID, std::string> ResourcePaths::mSounds;

    void ResourcePaths::loadFromJSON(const std::string& filename) {

        // Шаг 1. Низкоуровневое чтение файла (I/O-уровень) через FileLoader
        const auto fileContentOpt = FileLoader::loadTextFile(filename);
        if (!fileContentOpt) {
            // Это критический для движка файл: без него ресурсные ID не разрешатся.
            message::showError("[ResourcePaths]\nНе удалось открыть реестр ресурсов: " + filename);
            std::exit(EXIT_FAILURE);
        }

        const std::string& fileContent = *fileContentOpt;

        // Шаг 2. Разбор JSON
        Json data;
        try {
            data = Json::parse(fileContent);
        } catch (const std::exception& e) {
            message::showError(std::string("[ResourcePaths]\nОшибка парсинга JSON в ") + filename +
                               ": " + e.what());
            std::exit(EXIT_FAILURE);
        } catch (...) {
            message::showError("[ResourcePaths]\nНеизвестная ошибка при парсинге JSON: " +
                               filename);
            std::exit(EXIT_FAILURE);
        }

        // Шаг 3. Валидация верхнего уровня структуры
        try {
            validateResourceRegistry(data);
        } catch (const std::exception& e) {
            message::showError(std::string("[ResourcePaths]\nНеверная структура JSON: ") +
                               e.what());
            std::exit(EXIT_FAILURE);
        }

        // Шаг 4. Заполнение внутренних мапов
        loadTextureMap(data, mTextures);
        loadFontMap(data, mFonts);
        loadSoundMap(data, mSounds);
    }

    const std::string& ResourcePaths::get(ids::TextureID id) {
        auto it = mTextures.find(id);
        if (it == mTextures.end()) {
            throw std::runtime_error(
                "[ResourcePaths::get(TextureID)]\nНе найден путь для TextureID: " +
                std::string(toString(id)));
        }
        return it->second;
    }

    const std::string& ResourcePaths::get(ids::FontID id) {
        auto it = mFonts.find(id);
        if (it == mFonts.end()) {
            throw std::runtime_error("[ResourcePaths::get(FontID)]\nНе найден путь для FontID: " +
                                     std::string(toString(id)));
        }
        return it->second;
    }

    const std::string& ResourcePaths::get(ids::SoundID id) {
        auto it = mSounds.find(id);
        if (it == mSounds.end()) {
            throw std::runtime_error("[ResourcePaths::get(SoundID)]\nНе найден путь для SoundID: " +
                                     std::string(toString(id)));
        }
        return it->second;
    }

} // namespace core::resources::paths