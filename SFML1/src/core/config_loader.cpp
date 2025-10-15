#include <fstream>
#include <stdexcept>
#include "third_party/json_silent.hpp"
#include "core/config_loader.h"
#include "core/config.h"

using json = nlohmann::json;

// Универсальный парсер из JSON
namespace {

    // Базовый шаблон — fallback для типов, где нет спец. обработки
    template <typename T>
    T parseValue(const json& data, const std::string& key, const T& defaultValue) {
        if (data.contains(key))
            return data[key].get<T>();
        return defaultValue;
    }

    // Специализация для float (на случай, если будет int в JSON)
    template <>
    float parseValue<float>(const json& data, const std::string& key, const float& defaultValue) {
        if (data.contains(key)) {
            const auto& v = data[key];
            if (v.is_number())
                return v.get<float>();
        }
        return defaultValue;
    }

    // Специализация для std::string
    template <>
    std::string parseValue<std::string>(const json& data, const std::string& key, const std::string& defaultValue) {
        if (data.contains(key) && data[key].is_string())
            return data[key].get<std::string>();
        return defaultValue;
    }

    // Специализация для sf::Vector2f
    template <>
    sf::Vector2f parseValue<sf::Vector2f>(const json& data, const std::string& key, const sf::Vector2f& defaultValue) {
        if (!data.contains(key))
            return defaultValue;

        const auto& value = data[key];
        if (value.is_number()) {
            float v = value.get<float>();
            return { v, v };
        }
        if (value.is_array() && value.size() >= 2) {
            return { value[0].get<float>(), value[1].get<float>() };
        }
        if (value.is_object()) {
            float x = value.value("x", defaultValue.x);
            float y = value.value("y", defaultValue.y);
            return { x, y };
        }
        return defaultValue;
    }

} // namespace

namespace core {

        // Загружает JSON из файла
    PlayerConfig ConfigLoader::loadPlayerConfig(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open())
            throw std::runtime_error("ConfigLoader: не могу открыть файл " + path);

        // JSON-данные
        json data;
        file >> data;

        // Конфигурация игрока на основе JSON-данных
        PlayerConfig cfg; // Создаёт объект конфигурации игрока со значениями по по умолчанию

        // Загружает конфиг из JSON
        cfg.texture = parseValue<std::string>(data, "texture", cfg.texture);
        cfg.scale = parseValue<sf::Vector2f>(data, "scale", cfg.scale);
        cfg.speed = parseValue<float>(data, "speed", cfg.speed);
        cfg.startPosition = parseValue<sf::Vector2f>(data, "start_position", cfg.startPosition);
        cfg.resizeBehavior = parseValue<std::string>(data, "resize_behavior", cfg.resizeBehavior);

        return cfg;
    }

} // namespace core