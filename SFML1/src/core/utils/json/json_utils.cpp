#include "pch.h"

#include "core/utils/json/json_utils.h"

// Внутренняя утилита для парсинга цвета, её внешний API parseColor() - ниже
namespace {
    using core::utils::json::json;

    sf::Color parseColorValue(const json& value, const sf::Color& fallback) {
        // Для строки формата "#RRGGBB" или "#RRGGBBAA"
        if (value.is_string()) {
            std::string s = value.get<std::string>();
            if (!s.empty() && s[0] == '#') {
                s.erase(0, 1);

                auto hexToByte = [](const std::string& hex) -> uint8_t {
                    return static_cast<uint8_t>(std::stoul(hex, nullptr, 16));
                };

                try {
                    if (s.size() == 6) {
                        return {hexToByte(s.substr(0, 2)), hexToByte(s.substr(2, 2)),
                                hexToByte(s.substr(4, 2)), 255};
                    }
                    if (s.size() == 8) {
                        return {hexToByte(s.substr(0, 2)), hexToByte(s.substr(2, 2)),
                                hexToByte(s.substr(4, 2)), hexToByte(s.substr(6, 2))};
                    }
                } catch (...) {
                    return fallback;
                }
            }
        }

        // Для строки формата RGB: {"r": 255, "g": 0, "b": 0, "a": 255}
        else if (value.is_object()) {
            auto r = static_cast<uint8_t>(value.value("r", 255));
            auto g = static_cast<uint8_t>(value.value("g", 255));
            auto b = static_cast<uint8_t>(value.value("b", 255));
            auto a = static_cast<uint8_t>(value.value("a", 255));
            return {r, g, b, a};
        }

        return fallback;
    }
} // namespace

namespace core::utils::json {

    using json = nlohmann::json;

    // Базовый шаблон — для типов без спец. обработки
    template <typename T>
    T parseValue(const json& data, const std::string& key, const T& defaultValue) {
        if (data.contains(key)) {
            return data[key].get<T>();
        }
        return defaultValue;
    }

    // Явные специализации

    template <>
    float parseValue<float>(const json& data, const std::string& key, const float& defaultValue) {
        if (data.contains(key)) {
            const auto& v = data[key];
            if (v.is_number()) {
                return v.get<float>();
            }
        }
        return defaultValue;
    }

    template <>
    std::string
    parseValue<std::string>(const json& data,
                            const std::string& key, // NOLINT(bugprone-easily-swappable-parameters)
                            const std::string& defaultValue) {
        if (data.contains(key) && data[key].is_string()) {
            return data[key].get<std::string>();
        }
        return defaultValue;
    }

    template <>
    sf::Vector2f parseValue<sf::Vector2f>(const json& data, const std::string& key,
                                          const sf::Vector2f& defaultValue) {
        if (!data.contains(key)) {
            return defaultValue;
        }

        const auto& value = data[key];

        if (value.is_number()) {
            float v = value.get<float>();
            return {v, v};
        }
        if (value.is_array() && value.size() >= 2) {
            return {value.at(0).get<float>(), value.at(1).get<float>()};
        }
        if (value.is_object()) {
            float x = value.value("x", defaultValue.x);
            float y = value.value("y", defaultValue.y);
            return {x, y};
        }
        return defaultValue;
    }

    // Преобразует строковое имя клавиши (напр. "W", "Left") в sf::Keyboard::Key.
    // Используется в parseKey и ConfigLoader при обработке controls.
    sf::Keyboard::Key parseKeyString(const std::string& name) {
        using K = sf::Keyboard::Key;

        if (name == "W") {
            return K::W;
        }
        if (name == "A") {
            return K::A;
        }
        if (name == "S") {
            return K::S;
        }
        if (name == "D") {
            return K::D;
        }
        if (name == "Up") {
            return K::Up;
        }
        if (name == "Down") {
            return K::Down;
        }
        if (name == "Left") {
            return K::Left;
        }
        if (name == "Right") {
            return K::Right;
        }
        if (name == "Space") {
            return K::Space;
        }
        if (name == "LShift") {
            return K::LShift;
        }
        if (name == "RShift") {
            return K::RShift;
        }
        if (name == "LControl") {
            return K::LControl;
        }
        if (name == "RControl") {
            return K::RControl;
        }
        if (name == "LAlt") {
            return K::LAlt;
        }
        if (name == "RAlt") {
            return K::RAlt;
        }

        // можно дополнять по мере надобности
        return K::Unknown;
    }

    // Преобразование строки из JSON-объекта в клавишу SFML.
    sf::Keyboard::Key parseKey(const json& data, const std::string& key,
                               sf::Keyboard::Key defaultValue) {
        // Если нет поля или неверный формат → возвращаем defaultValue.
        if (!data.contains(key) || !data[key].is_string()) {
            return defaultValue;
        }
        const std::string name = data[key].get<std::string>();
        return parseKeyString(name);
    }

    // Парсинг цвета из JSON
    sf::Color parseColor(const json& data, const std::string& key, const sf::Color& defaultValue) {
        // Если в JSON нет "color" → возвращаем → возвращаем defaultValue.
        if (!data.contains(key)) {
            return defaultValue;
        }
        return parseColorValue(data.at(key), defaultValue);
    }

    // Явные инстанциации, чтобы линковщик видел шаблоны
    template float parseValue<float>(const json&, const std::string&, const float&);
    template std::string parseValue<std::string>(const json&, const std::string&,
                                                 const std::string&);
    template sf::Vector2f parseValue<sf::Vector2f>(const json&, const std::string&,
                                                   const sf::Vector2f&);

} // namespace core::utils::json