// ================================================================================================
// File: core/utils/json/json_document.h
// Purpose: JSON parsing + (optional) structural validation helpers for config loading.
// Used by: Config loaders (engine/game), resource registries, bootstrap code.
// Related headers:
//  - core/utils/json/json_common.h
//  - core/utils/json/json_validator.h   // only for SchemaPolicy::Validate / KeyRule API
//  - adapters/json/json_silent.hpp
//  - core/log/log_macros.h
// Notes:
//  - Not a DOM wrapper. "Document" means a loaded JSON text file.
//  - Duplicate key policy is enforced at parse-time via callback.
// ================================================================================================
#pragma once

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "core/utils/json/json_common.h"
#include "core/utils/json/json_validator.h"

namespace core::utils::json {

    /**
     * @brief Политика обработки дубликатов ключей JSON-объектов.
     *
     * Важно: после парсинга в DOM (nlohmann::json) дубликаты ключей уже не обнаружить,
     * т.к. остаётся только "последнее значение". Поэтому режим Reject требует
     * детектора на этапе парсинга (callback).
     */
    enum class DuplicateKeyPolicy : std::uint8_t {
        Reject, // считать дубликаты ключей ошибкой
        Ignore  // игнорировать (быстрее, подходит для программно-генерируемых данных/сейвов)
    };

    /**
     * @brief Политика проверки структуры JSON по "правилам" (schema-like).
     */
    enum class SchemaPolicy : std::uint8_t {
        Validate, // проверять наличие ключей/типы по rules
        Skip      // пропустить проверку (быстрее)
    };

    /**
     * @brief Опции парсинга/валидации JSON-документа.
     */
    struct JsonParseOptions final {
        DuplicateKeyPolicy duplicateKeys = DuplicateKeyPolicy::Reject;
        SchemaPolicy schema = SchemaPolicy::Validate;
    };

    /**
     * @brief Пресет для конфигов (ручное authoring-time редактирование).
     * Дубликаты ключей запрещены, схема валидируется.
     */
    inline constexpr JsonParseOptions kConfigStrictOptions{
        .duplicateKeys = DuplicateKeyPolicy::Reject, .schema = SchemaPolicy::Validate};

    /**
     * @brief Пресет для конфигов:
     * дубликаты ключей запрещены, но schema-проверка отключена,
     * потому что вся диагностика делается через парсеры *WithIssue.
     */
    inline constexpr JsonParseOptions kConfigParseOnlyOptions{
        .duplicateKeys = DuplicateKeyPolicy::Reject, .schema = SchemaPolicy::Skip};

    /**
     * @brief Пресет для "быстрого" режима (сейвы/стриминг/программная генерация).
     * Дубликаты не проверяем, схему можно пропускать.
     */
    inline constexpr JsonParseOptions kFastOptions{.duplicateKeys = DuplicateKeyPolicy::Ignore,
                                                   .schema = SchemaPolicy::Skip};

    // --------------------------------------------------------------------------------------------
    // Базовый API (явные опции)
    // --------------------------------------------------------------------------------------------

    // Парсинг JSON-текста + (опциональная) проверка структуры.
    // Критический режим: LOG_PANIC; при ошибке завершает процесс.
    [[nodiscard]] json parseAndValidateCritical(const std::string& fileContent,
                                                std::string_view path, std::string_view moduleTag,
                                                std::span<const JsonValidator::KeyRule> rules,
                                                JsonParseOptions options);

    // Парсинг JSON-текста + (опциональная) проверка структуры.
    // Некритический режим: LOG_DEBUG + возврат nullopt при ошибке.
    [[nodiscard]] std::optional<json> parseAndValidateNonCritical(
        const std::string& fileContent,
        [[maybe_unused]] std::string_view path,
        [[maybe_unused]] std::string_view moduleTag,
        std::span<const JsonValidator::KeyRule> rules, JsonParseOptions options);

    // --------------------------------------------------------------------------------------------
    // Backward-compatible overloads (по умолчанию — строгий режим для конфигов)
    // --------------------------------------------------------------------------------------------

    [[nodiscard]] inline json
    parseAndValidateCritical(const std::string& fileContent, std::string_view path,
                             std::string_view moduleTag,
                             std::span<const JsonValidator::KeyRule> rules) {
        return parseAndValidateCritical(fileContent, path, moduleTag, rules, kConfigStrictOptions);
    }

    [[nodiscard]] inline std::optional<json>
    parseAndValidateNonCritical(const std::string& fileContent, std::string_view path,
                                std::string_view moduleTag,
                                std::span<const JsonValidator::KeyRule> rules) {
        return parseAndValidateNonCritical(fileContent, path, moduleTag, rules,
                                           kConfigStrictOptions);
    }

    // --------------------------------------------------------------------------------------------
    // Перегрузки для синтаксиса { rule1, rule2, ... } без явного std::vector
    // --------------------------------------------------------------------------------------------

    [[nodiscard]] inline json parseAndValidateCritical(
        const std::string& fileContent, std::string_view path, std::string_view moduleTag,
        std::initializer_list<JsonValidator::KeyRule> rules, JsonParseOptions options) {
        return parseAndValidateCritical(fileContent, path, moduleTag,
                                        std::span(rules.begin(), rules.end()), options);
    }

    [[nodiscard]] inline std::optional<json> parseAndValidateNonCritical(
        const std::string& fileContent, std::string_view path, std::string_view moduleTag,
        std::initializer_list<JsonValidator::KeyRule> rules, JsonParseOptions options) {
        return parseAndValidateNonCritical(fileContent, path, moduleTag,
                                           std::span(rules.begin(), rules.end()), options);
    }

    [[nodiscard]] inline json
    parseAndValidateCritical(const std::string& fileContent, std::string_view path,
                             std::string_view moduleTag,
                             std::initializer_list<JsonValidator::KeyRule> rules) {
        return parseAndValidateCritical(fileContent, path, moduleTag,
                                        std::span(rules.begin(), rules.end()),
                                        kConfigStrictOptions);
    }

    [[nodiscard]] inline std::optional<json>
    parseAndValidateNonCritical(const std::string& fileContent, std::string_view path,
                                std::string_view moduleTag,
                                std::initializer_list<JsonValidator::KeyRule> rules) {
        return parseAndValidateNonCritical(fileContent, path, moduleTag,
                                           std::span(rules.begin(), rules.end()),
                                           kConfigStrictOptions);
    }

} // namespace core::utils::json