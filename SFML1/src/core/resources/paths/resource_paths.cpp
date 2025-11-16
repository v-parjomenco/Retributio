#include "pch.h"

#include "core/resources/paths/resource_paths.h"
#include <fstream>
#include <stdexcept>

#include "third_party/json_silent.hpp"

#include "core/utils/json/json_validator.h"

using json = nlohmann::json;

namespace core::resources::paths {

    std::unordered_map<ids::TextureID, std::string> ResourcePaths::mTextures;
    std::unordered_map<ids::FontID, std::string> ResourcePaths::mFonts;
    std::unordered_map<ids::SoundID, std::string> ResourcePaths::mSounds;

    // Валидация структуры JSON
    void ResourcePaths::validateJSON(const json& data) {
        std::vector<utils::json::JsonValidator::KeyRule> rules = {
            {"textures", {json::value_t::object}, true},
            {"fonts", {json::value_t::object}, true},
            {"sounds", {json::value_t::object}, false} // пока звуков нет, необязательно
        };
        utils::json::JsonValidator::validate(data, rules);
    }

    void ResourcePaths::loadFromJSON(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("[ResourcePaths::loadFromJSON]\nНе удалось открыть JSON: " +
                                     filename);
        }

        json data;
        file >> data;

        validateJSON(data); // валидируем структуру загруженных из JSON данных

        mTextures.clear();
        mFonts.clear();
        mSounds.clear();

        if (data.contains("textures") && data["textures"].is_object()) {
            if (data["textures"].contains("Player")) {
                mTextures[ids::TextureID::Player] = data["textures"]["Player"];
            }
        }

        if (data.contains("fonts") && data["fonts"].is_object()) {
            if (data["fonts"].contains("Default")) {
                mFonts[ids::FontID::Default] = data["fonts"]["Default"];
            }
        }

        if (data.contains("sounds") && data["sounds"].is_object()) {
            // Пока звуков нет
        }
    }

    const std::string& ResourcePaths::get(ids::TextureID id) {
        auto it = mTextures.find(id);
        if (it == mTextures.end()) {
            throw std::runtime_error("[ResourcePaths::get]\nНе найден путь для TextureID: " +
                                     std::string(toString(id)));
        }
        return it->second;
    }

    const std::string& ResourcePaths::get(ids::FontID id) {
        auto it = mFonts.find(id);
        if (it == mFonts.end()) {
            throw std::runtime_error("[ResourcePaths::get]\nНе найден путь для FontID: " +
                                     std::string(toString(id)));
        }
        return it->second;
    }

    const std::string& ResourcePaths::get(ids::SoundID id) {
        auto it = mSounds.find(id);
        if (it == mSounds.end()) {
            throw std::runtime_error("[ResourcePaths::get]\nНе найден путь для SoundID: " +
                                     std::string(toString(id)));
        }
        return it->second;
    }

} // namespace core::resources::paths