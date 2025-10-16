#include "core/json_utils.h"

namespace core::json_utils {

    using json = nlohmann::json;

    // Базовый шаблон — для типов без спец. обработки
    template <typename T>
    T parseValue(const json& data, const std::string& key, const T& defaultValue) {
        if (data.contains(key))
            return data[key].get<T>();
        return defaultValue;
    }

    // Явные специализации

    template <>
    float parseValue<float>(const json& data, const std::string& key, const float& defaultValue) {
        if (data.contains(key)) {
            const auto& v = data[key];
            if (v.is_number())
                return v.get<float>();
        }
        return defaultValue;
    }

    template <>
    std::string parseValue<std::string>(const json& data, const std::string& key, const std::string& defaultValue) {
        if (data.contains(key) && data[key].is_string())
            return data[key].get<std::string>();
        return defaultValue;
    }

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

    // Явные инстанциации, чтобы линковщик видел шаблоны
    template float parseValue<float>(const json&, const std::string&, const float&);
    template std::string parseValue<std::string>(const json&, const std::string&, const std::string&);
    template sf::Vector2f parseValue<sf::Vector2f>(const json&, const std::string&, const sf::Vector2f&);

} // namespace core::json_utils