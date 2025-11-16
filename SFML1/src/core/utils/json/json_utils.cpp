#include "pch.h"
#include "core/utils/json/json_utils.h"

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
    // Если нет поля или неверный формат, возвращает defaultValue.
    sf::Keyboard::Key parseKey(const json& data, const std::string& key,
                               sf::Keyboard::Key defaultValue) {
        if (!data.contains(key) || !data[key].is_string()) {
            return defaultValue;
        }

        const std::string name = data[key].get<std::string>();
        return parseKeyString(name);
    }

    // Явные инстанциации, чтобы линковщик видел шаблоны
    template float parseValue<float>(const json&, const std::string&, const float&);
    template std::string parseValue<std::string>(const json&, const std::string&,
                                                 const std::string&);
    template sf::Vector2f parseValue<sf::Vector2f>(const json&, const std::string&,
                                                   const sf::Vector2f&);

} // namespace core::utils::json