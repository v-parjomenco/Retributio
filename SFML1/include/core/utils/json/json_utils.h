// ================================================================================================
// File: core/utils/json/json_utils.h
// Purpose: Typed helpers for reading values from nlohmann::json +
//          shared helpers for parsing and validating JSON configs
// Used by: ConfigLoader, DebugOverlayBlueprint, other config parsers
// ================================================================================================

#pragma once

#include <optional>
#include <string>
#include <vector>

#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Keyboard.hpp>

#include "third_party/json_silent.hpp"
#include "core/utils/json/json_validator.h"

namespace core::utils::json {

    using json = nlohmann::json;

    // --------------------------------------------------------------------------------------------
    // Шаблонный парсер и его специализации
    // --------------------------------------------------------------------------------------------

    /**
    * @brief Универсальный шаблон для чтения значения из JSON.
    * Если ключ присутствует и тип совместим, берём T из JSON.
    * Если ключ отсутствует — возвращаем defaultValue.
    * Для некоторых типов (float, std::string, sf::Vector2f, bool, unsigned)
    * есть специализации с более "умной" логикой.
    */
    template <typename T>
    T parseValue(const json& data, const std::string& key, const T& defaultValue);

    // Специализации — объявляются здесь, реализуются в .cpp.
    
    // float: аккуратная проверка на number
    template <>
    float parseValue<float>(const json& data, const std::string& key, const float& defaultValue);

    // std::string: только строка, иначе default
    template <>
    std::string parseValue<std::string>(const json& data,
                                        const std::string& key,
                                        const std::string& defaultValue);

    // sf::Vector2f: поддержка форматов:
    //  - число          → (v, v)
    //  - массив [x, y]  → (x, y)
    //  - объект {x, y}  → (x, y)
    template <>
    sf::Vector2f parseValue<sf::Vector2f>(const json& data,
                                          const std::string& key,
                                          const sf::Vector2f& defaultValue);

    // bool: безопасное чтение логического значения
    template <>
    bool parseValue<bool>(const json& data, const std::string& key, const bool& defaultValue);

    // unsigned: целое без знака (используется, например, для characterSize)
    template <>
    unsigned parseValue<unsigned>(const json& data,
                                  const std::string& key,
                                  const unsigned& defaultValue);

    /**
    * @brief Generic helper для enum-типов, задаваемых строкой в JSON.
    *
    * Предполагается, что:
    *  - Enum — это enum/enum class (например, TextureID),
    *  - mapper — функция/functor вида
    *        std::optional<Enum>(std::string_view)
    *    (например, textureFromString / fontFromString),
    *  - JSON хранит строковый идентификатор (например, "Player"), а не путь.
    *
    * Поведение:
    *  - если ключ отсутствует или не является строкой → возвращается defaultValue;
    *  - если mapper(name) вернул значение → оно используется;
    *  - если mapper(name) вернул std::nullopt → возвращается defaultValue.
    *
    * Важно:
    *  - parseEnum НИЧЕГО не логирует и не показывает ошибок пользователю;
    *    он только инкапсулирует типовой паттерн "string → enum с fallback".
    *  - В местах, где нужен popup/лог (как в ConfigLoader), можно использовать
    *    этот helper как building block, но добавлять сообщения об ошибках отдельно.
    */
    template <typename Enum, typename Mapper>
    Enum parseEnum(const json& data, const std::string& key, Enum defaultValue, Mapper mapper) {
        // Нет ключа или не строка — просто возвращаем дефолт.
        if (!data.contains(key) || !data.at(key).is_string()) {
            return defaultValue;
        }

        const std::string name = data.at(key).get<std::string>();

        // Ожидаем, что mapper вернёт std::optional<Enum>.
        if (auto idOpt = mapper(name)) {
            return *idOpt;
        }

        // Не удалось сопоставить строку с enum — используем дефолт.
        return defaultValue;
    }

    // --------------------------------------------------------------------------------------------
    // Специализированные парсеры
    // --------------------------------------------------------------------------------------------

    // Преобразование "controls" в sf::Keyboard::Key.
    sf::Keyboard::Key parseKey(const json& data, const std::string& key,
                               sf::Keyboard::Key defaultValue);
    // Парсинг цвета:
    //  - "#RRGGBB" / "#RRGGBBAA"
    //  - объект { "r": 255, "g": 0, "b": 0, "a": 255 }
    sf::Color parseColor(const json& data, const std::string& key,
                         const sf::Color& defaultValue);

    // --------------------------------------------------------------------------------------------
    // Общие хелперы для парсинга и валидации JSON-конфигов
    // --------------------------------------------------------------------------------------------

    /**
    * @brief parseAndValidateCritical(...)
    * 
    * - Используется для "критичных" конфигов (player.json и т.п.).
    * - При ошибке парсинга или валидации:
    *   * показывает пользователю окно ошибки (showError),
    *   * вызывает std::exit(EXIT_FAILURE).
    * - Если всё хорошо — возвращает разобранный json.
    * moduleTag - строка вроде "ConfigLoader" / "DebugOverlayBlueprint" — для префикса в сообщениях.
    */
    json parseAndValidateCritical(const std::string& fileContent,
                                  const std::string& path,
                                  const char* moduleTag,
                                  const std::vector<JsonValidator::KeyRule>& rules);

    /**
    * @brief parseAndValidateNonCritical(...)
    * 
    * - Используется для не критичных вещей (debug overlay, вспомогательные настройки).
    * - При ошибке:
    *   * пишет logDebug,
    *   * возвращает std::nullopt (вызывающий код сам решает, что делать дальше).
    * moduleTag - строка вроде "ConfigLoader" / "DebugOverlayBlueprint" — для префикса в сообщениях.
    */
    std::optional<json>
    parseAndValidateNonCritical(const std::string& fileContent,
                                const std::string& path,
                                const char* moduleTag,
                                const std::vector<JsonValidator::KeyRule>& rules);

} // namespace core::utils::json