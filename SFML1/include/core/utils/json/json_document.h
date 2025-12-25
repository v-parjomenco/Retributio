// ================================================================================================
// File: core/utils/json/json_document.h
// Purpose: JSON document parsing + validation pipeline
// (NOT a DOM wrapper — just utility functions).
//          Provides parseAndValidateCritical/NonCritical for config loading.
// Used by: ConfigLoader, DebugOverlayLoader, WindowConfigLoader, EngineSettingsLoader.
// Notes:
//  - This is NOT a document class — just parsing+validation helper functions.
//  - "Document" refers to the conceptual JSON config file being loaded.
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
        .duplicateKeys = DuplicateKeyPolicy::Reject,
        .schema = SchemaPolicy::Validate
    };

    /**
     * @brief Пресет для "быстрого" режима (сейвы/стриминг/программная генерация).
     * Дубликаты не проверяем, схему можно пропускать.
     */
    inline constexpr JsonParseOptions kFastOptions{
        .duplicateKeys = DuplicateKeyPolicy::Ignore,
        .schema = SchemaPolicy::Skip
    };

    // --------------------------------------------------------------------------------------------
    // Базовый API (явные опции)
    // --------------------------------------------------------------------------------------------

    // Парсинг JSON-текста + (опциональная) проверка структуры.
    // Критический режим: LOG_PANIC; при ошибке завершает процесс.
    [[nodiscard]] json
    parseAndValidateCritical(const std::string& fileContent,
                             std::string_view path,
                             std::string_view moduleTag,
                             std::span<const JsonValidator::KeyRule> rules,
                             JsonParseOptions options);

    // Парсинг JSON-текста + (опциональная) проверка структуры.
    // Некритический режим: LOG_DEBUG + возврат nullopt при ошибке.
    [[nodiscard]] std::optional<json>
    parseAndValidateNonCritical(const std::string& fileContent,
                                std::string_view path,
                                std::string_view moduleTag,
                                std::span<const JsonValidator::KeyRule> rules,
                                JsonParseOptions options);

    // --------------------------------------------------------------------------------------------
    // Backward-compatible overloads (по умолчанию — строгий режим для конфигов)
    // --------------------------------------------------------------------------------------------

    [[nodiscard]] inline json
    parseAndValidateCritical(const std::string& fileContent,
                             std::string_view path,
                             std::string_view moduleTag,
                             std::span<const JsonValidator::KeyRule> rules) {
        return parseAndValidateCritical(fileContent, path, moduleTag, rules, kConfigStrictOptions);
    }

    [[nodiscard]] inline std::optional<json>
    parseAndValidateNonCritical(const std::string& fileContent,
                                std::string_view path,
                                std::string_view moduleTag,
                                std::span<const JsonValidator::KeyRule> rules) {
        return parseAndValidateNonCritical(fileContent, path, moduleTag, rules, kConfigStrictOptions);
    }

    // --------------------------------------------------------------------------------------------
    // Перегрузки для синтаксиса { rule1, rule2, ... } без явного std::vector
    // --------------------------------------------------------------------------------------------

    [[nodiscard]] inline json
    parseAndValidateCritical(const std::string& fileContent,
                             std::string_view path,
                             std::string_view moduleTag,
                             std::initializer_list<JsonValidator::KeyRule> rules,
                             JsonParseOptions options) {
        return parseAndValidateCritical(fileContent,
                                        path,
                                        moduleTag,
                                        std::span(rules.begin(), rules.end()),
                                        options);
    }

    [[nodiscard]] inline std::optional<json>
    parseAndValidateNonCritical(const std::string& fileContent,
                                std::string_view path,
                                std::string_view moduleTag,
                                std::initializer_list<JsonValidator::KeyRule> rules,
                                JsonParseOptions options) {
        return parseAndValidateNonCritical(fileContent,
                                           path,
                                           moduleTag,
                                           std::span(rules.begin(), rules.end()),
                                           options);
    }

    [[nodiscard]] inline json
    parseAndValidateCritical(const std::string& fileContent,
                             std::string_view path,
                             std::string_view moduleTag,
                             std::initializer_list<JsonValidator::KeyRule> rules) {
        return parseAndValidateCritical(fileContent,
                                        path,
                                        moduleTag,
                                        std::span(rules.begin(), rules.end()),
                                        kConfigStrictOptions);
    }

    [[nodiscard]] inline std::optional<json>
    parseAndValidateNonCritical(const std::string& fileContent,
                                std::string_view path,
                                std::string_view moduleTag,
                                std::initializer_list<JsonValidator::KeyRule> rules) {
        return parseAndValidateNonCritical(fileContent,
                                           path,
                                           moduleTag,
                                           std::span(rules.begin(), rules.end()),
                                           kConfigStrictOptions);
    }

} // namespace core::utils::json
