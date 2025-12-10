// ================================================================================================
// File: core/utils/json/json_utils.h
// Purpose: Typed helpers for reading values from nlohmann::json +
//          shared helpers for parsing and validating JSON configs
// Used by: ConfigLoader, DebugOverlayBlueprint, other config parsers
// ================================================================================================
#pragma once

#include <exception>
#include <optional>
#include <string>
#include <string_view>
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
     *
     * Если ключ присутствует и тип совместим, берём T из JSON.
     * Если ключ отсутствует или чтение/конвертация не удалась — возвращаем defaultValue.
     *
     * Для некоторых типов (float, std::string, sf::Vector2f, bool, unsigned)
     * есть специализации с более "умной" логикой (без исключений и с тонкой проверкой типов).
     */
    template <typename T>
    T parseValue(const json& data, std::string_view key, const T& defaultValue) {
        if (!data.contains(key)) {
            return defaultValue;
        }

        try {
            return data.at(key).get<T>();
        } catch (const std::exception&) {
            // Generic-случай: не логируем и не бросаем.
            // JSON в этом месте считается "мягким" — просто возвращаем fallback.
            return defaultValue;
        }
    }

    // --------------------------------------------------------------------------------------------
    // Специализации parseValue<T>
    // --------------------------------------------------------------------------------------------

    // float: аккуратная проверка на number
    template <>
    float parseValue<float>(const json& data,
                            std::string_view key,
                            const float& defaultValue);

    // std::string: только строка, иначе default
    template <>
    std::string parseValue<std::string>(const json& data,
                                        std::string_view key,
                                        const std::string& defaultValue);

    // sf::Vector2f: поддержка форматов:
    //  - число          → (v, v)
    //  - массив [x, y]  → (x, y)
    //  - объект {x, y}  → (x, y)
    template <>
    sf::Vector2f parseValue<sf::Vector2f>(const json& data,
                                          std::string_view key,
                                          const sf::Vector2f& defaultValue);

    // bool: безопасное чтение логического значения
    template <>
    bool parseValue<bool>(const json& data,
                          std::string_view key,
                          const bool& defaultValue);

    // unsigned: целое без знака (используется, например, для characterSize)
    template <>
    unsigned parseValue<unsigned>(const json& data,
                                  std::string_view key,
                                  const unsigned& defaultValue);

    /**
     * @brief Generic helper для enum-типов, задаваемых строкой в JSON.
     *
     * Ожидается, что:
     *  - Enum — это enum / enum class (например, TextureID),
     *  - mapper — функция/functor вида std::optional<Enum>(std::string_view),
     *  - JSON хранит строковый идентификатор (например, "Player"), а не путь.
     *
     * Поведение:
     *  - если ключ отсутствует или не является строкой → возвращается defaultValue;
     *  - если mapper(name) вернул значение → оно используется;
     *  - если mapper(name) вернул std::nullopt → вызывается onInvalidValue(name),
     *    затем возвращается defaultValue.
     *
     * ВАЖНО:
     *  - parseEnum сам делает zero-copy доступ к строке внутри JSON через get_ref;
     *  - ErrorHandler вызывается только при реально нераспознанном значении;
     *  - отсутствующий ключ / неправильный тип считаются "мягкой" ситуацией
     *    и не считаются ошибкой значения.
     *
     * Логика логирования/попапов целиком остаётся на стороне вызывающего кода.
     */
    template <typename Enum, typename Mapper, typename ErrorHandler>
    Enum parseEnum(const json& data,
                   const char* key,
                   Enum defaultValue,
                   Mapper mapper,
                   ErrorHandler onInvalidValue) {
        // Пытаемся найти ключ без выбрасывания исключений.
        const auto it = data.find(key);
        if (it == data.end() || !it->is_string()) {
            // Ключа нет или не строка — трактуем как "нет значения", оставляем дефолт.
            return defaultValue;
        }

        // Zero-copy доступ к строке внутри JSON.
        const auto& nameRef = it->get_ref<const std::string&>();
        const std::string_view name{nameRef};

        // Ожидаем, что mapper вернёт std::optional<Enum>.
        if (auto idOpt = mapper(name)) {
            return *idOpt;
        }

        // Строка была, но mapper её не распознал → даём вызывающему залогировать.
        onInvalidValue(name);
        return defaultValue;
    }

    /**
     * @brief Упрощённый вариант parseEnum без логирования.
     *
     * Полезен там, где достаточно тихого fallback'а:
     *  - значение отсутствует → defaultValue;
     *  - значение некорректно → defaultValue без логов.
     */
    template <typename Enum, typename Mapper>
    Enum parseEnum(const json& data, const char* key, Enum defaultValue, Mapper mapper) {
        return parseEnum(
            data,
            key,
            defaultValue,
            mapper,
            [](std::string_view) {
                // silent fallback
            });
    }

    /**
     * @brief Безопасное чтение опционального поля.
     *
     * Семантика:
     *  - если ключа нет -> std::nullopt;
     *  - если есть, но тип/значение некорректно -> std::nullopt (и опциональный logDebug);
     *  - исключения наружу не выбрасываются.
     *
     * Ожидается, что структурную валидацию (наличие ключа, тип) уже сделал JsonValidator.
     */
    template <typename T>
    std::optional<T> getOptional(const json& data, const char* key) {
        if (!data.contains(key)) {
            return std::nullopt;
        }

        try {
            const auto& node = data.at(key);
            return node.get<T>();
        } catch (const std::exception& /*e*/) {
            // Здесь при желании можно вызвать logDebug, но без showError/std::exit.
            return std::nullopt;
        }
    }

    // --------------------------------------------------------------------------------------------
    // Специализации getOptional<T>
    // --------------------------------------------------------------------------------------------

    /**
     * @brief Специализация getOptional для std::string_view.
     *
     * ВАЖНО: Возвращаемый std::string_view ссылается на внутреннюю строку JSON.
     * JSON-объект `data` ДОЛЖЕН оставаться живым на протяжении использования string_view.
     *
     * Безопасно:
     *   const json data = loadConfig();
     *   if (auto sv = getOptional<std::string_view>(data, "key")) {
     *       process(*sv);  // data всё ещё жив
     *   }
     *
     * Опасно:
     *   auto sv = getOptional<std::string_view>(getTempJson(), "key");  // Dangling reference!
     *   auto sv2 = [&](){ json localJson = ...;
     *   return getOptional<std::string_view>(localJson, "key"); }();
     *   // sv2 продолжает жить за пределами жизни localJson → UB.
     */
    template <>
    inline std::optional<std::string_view> getOptional<std::string_view>(const json& data,
                                                                         const char* key) {
        const auto it = data.find(key);
        if (it == data.end() || !it->is_string()) {
            return std::nullopt;
        }

        const auto& ref = it->get_ref<const std::string&>();
        return std::string_view{ref};
    }

    // Запрещаем использование с временным json (rvalue), чтобы не было висячих ссылок.
    std::optional<std::string_view> getOptional(const json&&, const char*) = delete;

    // --------------------------------------------------------------------------------------------
    // Специализированные парсеры
    // --------------------------------------------------------------------------------------------

    // Преобразование "controls" в sf::Keyboard::Key.
    sf::Keyboard::Key parseKey(const json& data,
                               std::string_view key,
                               sf::Keyboard::Key defaultValue);

    // Парсинг цвета:
    //  - "#RRGGBB" / "#RRGGBBAA"
    //  - объект { "r": 255, "g": 0, "b": 0, "a": 255 }
    sf::Color parseColor(const json& data,
                         std::string_view key,
                         const sf::Color& defaultValue);

    // --------------------------------------------------------------------------------------------
    // Общие хелперы для парсинга и валидации JSON-конфигов
    // --------------------------------------------------------------------------------------------

    /**
     * @brief parseAndValidateCritical(...)
     *
     * - Используется для "критичных" конфигов (player.json и т.п.).
     * - При ошибке парсинга или валидации:
     *   * логирует подробности через core::log с категорией Config;
     *   * вызывает LOG_PANIC, который завершает процесс (std::exit(EXIT_FAILURE)).
     * - Если всё хорошо — возвращает разобранный json.
     *
     * moduleTag - строка вроде "ConfigLoader" / "DebugOverlayBlueprint" — для префикса в сообщениях.
     */
    [[nodiscard]] json
    parseAndValidateCritical(const std::string& fileContent,
                             std::string_view path,
                             const char* moduleTag,
                             const std::vector<JsonValidator::KeyRule>& rules);

    /**
     * @brief parseAndValidateNonCritical(...)
     *
     * - Используется для не критичных вещей (debug overlay, вспомогательные настройки).
     * - При ошибке:
     *   * пишет диагностическое сообщение в core::log (обычно Debug/Warn, категория Config),
     *   * возвращает std::nullopt (вызывающий код сам решает, что делать дальше).
     *
     * moduleTag - строка вроде "ConfigLoader" / "DebugOverlayBlueprint" — для префикса в сообщениях.
     */
    [[nodiscard]] std::optional<json>
    parseAndValidateNonCritical(const std::string& fileContent,
                                std::string_view path,
                                const char* moduleTag,
                                const std::vector<JsonValidator::KeyRule>& rules);

} // namespace core::utils::json