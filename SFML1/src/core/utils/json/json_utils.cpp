#include "pch.h"

#include "core/utils/json/json_utils.h"
#include <cstdlib>   // для std::exit, EXIT_FAILURE
#include <stdexcept> // для std::exception

#include "core/utils/message.h"

namespace {
    using core::utils::json::json;
    namespace message = core::utils::message;

    // --------------------------------------------------------------------------------------------
    // Внутренние утилиты для парсинга цвета и клавиш
    // --------------------------------------------------------------------------------------------

    /**
     * @brief Внутренняя утилита: парсинг JSON-значения в sf::Color.
     *
     * Поддерживает:
     *  - строку "#RRGGBB"  (альфа-канал = 255)
     *  - строку "#RRGGBBAA"
     *  - объект { "r": ..., "g": ..., "b": ..., "a": ... }
     *
     * При любой ошибке возвращает fallback.
     * Внешний API для пользователей — core::utils::json::parseColor(...).
     */
    sf::Color parseColorValue(const json& value, const sf::Color& fallback) {
        // Строка формата "#RRGGBB" или "#RRGGBBAA"
        if (value.is_string()) {
            std::string s = value.get<std::string>();
            if (!s.empty() && s[0] == '#') {
                s.erase(0, 1);

                auto hexToByte = [](const std::string& hex) -> std::uint8_t {
                    return static_cast<std::uint8_t>(std::stoul(hex, nullptr, 16));
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
                    // Любая ошибка в разборе — возвращаем fallback.
                    return fallback;
                }
            }
        }
        // Объект { "r": 255, "g": 0, "b": 0, "a": 255 }
        else if (value.is_object()) {
            auto r = static_cast<std::uint8_t>(value.value("r", 255));
            auto g = static_cast<std::uint8_t>(value.value("g", 255));
            auto b = static_cast<std::uint8_t>(value.value("b", 255));
            auto a = static_cast<std::uint8_t>(value.value("a", 255));
            return {r, g, b, a};
        }

        // Любой другой формат — оставляем цвет по умолчанию.
        return fallback;
    }

    /**
     * @brief Внутренний парсер: строка ("W", "Left", ...) -> sf::Keyboard::Key.
     *
     * Используется в parseKey и ConfigLoader при обработке controls.
     * Внешний API для пользователей — core::utils::json::parseKey(...).
     */
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

} // namespace

namespace core::utils::json {

    using json = nlohmann::json;

    // --------------------------------------------------------------------------------------------
    // Специализации parseValue<T>
    // --------------------------------------------------------------------------------------------

    template <>
    float parseValue<float>(const json& data, const std::string& key, const float& defaultValue) {
        if (!data.contains(key)) {
            return defaultValue;
        }

        const auto& v = data.at(key);
        if (v.is_number()) {
            return v.get<float>();
        }
        return defaultValue;
    }

    template <>
    std::string
    parseValue<std::string>(const json& data,
                            const std::string& key, // NOLINT(bugprone-easily-swappable-parameters)
                            const std::string& defaultValue) {
        if (data.contains(key) && data.at(key).is_string()) {
            return data.at(key).get<std::string>();
        }
        return defaultValue;
    }

    template <>
    sf::Vector2f parseValue<sf::Vector2f>(const json& data, const std::string& key,
                                          const sf::Vector2f& defaultValue) {
        if (!data.contains(key)) {
            return defaultValue;
        }

        const auto& value = data.at(key);
        // 1) Одно число → одинаковый масштаб по X и Y.
        if (value.is_number()) {
            float v = value.get<float>();
            return {v, v};
        }
        // 2) Массив [x, y]
        if (value.is_array() && value.size() >= 2) {
            return {value.at(0).get<float>(), value.at(1).get<float>()};
        }
        // 3) Объект { "x": ..., "y": ... }
        if (value.is_object()) {
            float x = value.value("x", defaultValue.x);
            float y = value.value("y", defaultValue.y);
            return {x, y};
        }
        return defaultValue;
    }

    template <>
    bool parseValue<bool>(const json& data, const std::string& key, const bool& defaultValue) {
        if (!data.contains(key)) {
            return defaultValue;
        }
        const auto& v = data.at(key);
        if (v.is_boolean()) {
            return v.get<bool>();
        }
        return defaultValue;
    }

    template <>
    unsigned parseValue<unsigned>(const json& data, const std::string& key,
                                  const unsigned& defaultValue) {
        if (!data.contains(key)) {
            return defaultValue;
        }
        const auto& v = data.at(key);

        // Предпочитаем корректный беззнаковый тип.
        if (v.is_number_unsigned()) {
            return v.get<unsigned>();
        }
        // Разрешаем также целое со знаком, но только если оно неотрицательное.
        if (v.is_number_integer()) {
            int tmp = v.get<int>();
            if (tmp >= 0) {
                return static_cast<unsigned>(tmp);
            }
        }
        return defaultValue;
    }

    // --------------------------------------------------------------------------------------------
    // Клавиатурный парсер
    // --------------------------------------------------------------------------------------------

    sf::Keyboard::Key parseKey(const json& data, const std::string& key,
                               sf::Keyboard::Key defaultValue) {
        // Если нет поля или это не строка → возвращаем defaultValue.
        if (!data.contains(key) || !data.at(key).is_string()) {
            return defaultValue;
        }
        const std::string name = data.at(key).get<std::string>();
        return parseKeyString(name);
    }

    // --------------------------------------------------------------------------------------------
    // Парсер цвета
    // --------------------------------------------------------------------------------------------

    sf::Color parseColor(const json& data, const std::string& key, const sf::Color& defaultValue) {
        // Если в JSON нет ключа → возвращаем defaultValue.
        if (!data.contains(key)) {
            return defaultValue;
        }
        return parseColorValue(data.at(key), defaultValue);
    }

    // --------------------------------------------------------------------------------------------
    // Общие хелперы для парсинга и валидации JSON-конфигов
    // --------------------------------------------------------------------------------------------

    json parseAndValidateCritical(const std::string& fileContent, std::string_view path,
                                  const char* moduleTag,
                                  const std::vector<JsonValidator::KeyRule>& rules) {
        json data;
        try {
            // Парсинг JSON из строки: текст -> дерево nlohmann::json.
            data = json::parse(fileContent);
        } catch (const std::exception& e) {
            message::showError("[" + std::string(moduleTag) + "]\nОшибка чтения JSON: " + e.what());
            std::exit(EXIT_FAILURE);
        } catch (...) {
            message::showError("[" + std::string(moduleTag) +
                               "]\nНеизвестная ошибка при чтении JSON файла: " + std::string(path));
            std::exit(EXIT_FAILURE);
        }

        try {
            // Валидация структуры JSON:
            //  - какие ключи есть/нет,
            //  - какие у них типы.
            JsonValidator::validate(data, rules);
        } catch (const std::exception& e) {
            message::showError("[" + std::string(moduleTag) +
                               "]\nНеверная структура JSON: " + e.what());
            std::exit(EXIT_FAILURE);
        }

        return data;
    }

    std::optional<json>
    parseAndValidateNonCritical(const std::string& fileContent, std::string_view path,
                                const char* moduleTag,
                                const std::vector<JsonValidator::KeyRule>& rules) {

        json data;
        try {
            // Парсинг JSON из строки для некритичных конфигов.
            data = json::parse(fileContent);
        } catch (const std::exception& e) {
            message::logDebug("[" + std::string(moduleTag) + "]\nНеверный JSON в файле " +
                              std::string(path) + ": " + e.what());
            return std::nullopt;
        } catch (...) {
            message::logDebug("[" + std::string(moduleTag) +
                              "]\nНеизвестная ошибка при разборе JSON: " + std::string(path));
            return std::nullopt;
        }

        try {
            // Валидация структуры:
            // Все поля необязательны (required = false),
            // но если они присутствуют, тип должен соответствовать ожиданиям.
            JsonValidator::validate(data, rules);
        } catch (const std::exception& e) {
            message::logDebug("[" + std::string(moduleTag) + "]\nВалидация: " + e.what());
            return std::nullopt;
        }

        return data;
    }

} // namespace core::utils::json