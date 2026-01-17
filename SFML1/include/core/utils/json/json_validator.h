// ================================================================================================
// File: core/utils/json/json_validator.h
// Purpose: Minimal schema-like structural validation helper for JSON configs.
// Used by: core/utils/json/json_document.h
// Notes:
//  - Contract: STRUCTURE ONLY (root/object, required keys, and allowed JSON value types).
//  - NOT a replacement for *WithIssue parsers. Semantic/range/cross-field validation belongs there.
//  - No logging, no UI, no engine-level dependencies. On violation throws std::runtime_error.
// ================================================================================================
#pragma once

#include <cstddef>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "adapters/json/json_silent.hpp"

#ifdef _MSC_VER
// C5045: "Spectre mitigation" шум при проходе по вектору правил.
// JsonValidator — чисто инфраструктурный слой, поэтому глушим ворнинг локально.
#pragma warning(push)
#pragma warning(disable : 5045)
#endif

namespace core::utils::json {

    /**
     * @brief Минимальный структурный валидатор JSON-конфигов (schema-like gate).
     *
     * ВАЖНО (контракт):
     *  - Это "структурный барьер" для SchemaPolicy::Validate: проверяет root/object, наличие ключей и типы.
     *  - Он НЕ должен дублировать *WithIssue парсеры:
     *      *WithIssue покрывают смысловую валидацию (диапазоны, зависимости полей, enum-мэппинг, etc.).
     *  - Он НЕ является "универсальной схемой" и не развивается в полноценный JSON Schema движок.
     *
     * Поведение:
     *  - При нарушении бросает std::runtime_error с человеко-читаемым текстом.
     *  - Внутри нет логирования (логирование — ответственность верхнего слоя).
     */
    class JsonValidator {
      public:
        using json = nlohmann::json;

        /**
         * @brief Правило для ключа верхнего уровня (top-level).
         *
         * name:
         *  - имя ключа (ожидается литерал/стабильная строка конфиг-формата).
         * allowedTypes:
         *  - список допустимых json::value_t для значения по ключу.
         * required:
         *  - если true — ключ обязан существовать.
         *
         * Примечание:
         *  - intentionally хранит allowedTypes как vector, потому что это cold-path (граница загрузки).
         *  - НЕ используйте это как "семантическую валидацию" — это только проверка типа.
         */
        struct KeyRule {
            std::string name;
            std::vector<json::value_t> allowedTypes;
            bool required = true;
        };

        /**
         * @brief Проверяет, что JSON содержит все обязательные ключи и правильные типы.
         *
         * @param data  JSON-объект (конфиг).
         * @param rules Список правил (std::span позволяет передавать vector/array без копирования).
         *
         * @throws std::runtime_error при нарушении хотя бы одного правила
         *         или если root не object.
         *
         * Примечание по дизайну:
         *  - Это intentionally "flat" валидатор: проверяет только ключи текущего объекта (top-level).
         *  - Структуру вложенных объектов/массивов проверяйте в *WithIssue парсерах.
         */
        static void validate(const json& data, std::span<const KeyRule> rules) {
            // Защита от передачи массива/null/примитива вместо конфига.
            if (!data.is_object()) {
                // '\n' в начале — сознательно: в логах/трейсах сообщение читается как отдельный блок.
                throw std::runtime_error("\nConfig validation error: root JSON is not an object");
            }

            for (const auto& rule : rules) {
                const auto it = data.find(rule.name);
                if (it == data.end()) {
                    if (rule.required) {
                        throw std::runtime_error("\nConfig validation error: missing key '" +
                                                 rule.name + "'");
                    }
                    continue;
                }

                const auto type = it->type();

                bool valid = false;
                for (const auto allowed : rule.allowedTypes) {
                    if (type == allowed) {
                        valid = true;
                        break;
                    }
                }

                if (!valid) {
                    std::string expected;
                    // Ошибка — cold-path. Reserve здесь чисто косметика, чтобы не плодить реаллокации.
                    expected.reserve(rule.allowedTypes.size() * 8);

                    for (std::size_t i = 0; i < rule.allowedTypes.size(); ++i) {
                        expected += typeName(rule.allowedTypes[i]);
                        if (i + 1 < rule.allowedTypes.size()) {
                            expected += " or ";
                        }
                    }

                    // std::string(...) нужен для конкатенации со строковыми литералами.
                    throw std::runtime_error("\nConfig validation error: key '" + rule.name +
                                             "' has wrong type (" + std::string(typeName(type)) +
                                             "), expected " + expected);
                }
            }
        }

      private:
        // Возвращаем string_view, так как литералы живут вечно (static storage).
        // Zero-overhead по сравнению с возвратом std::string.
        static std::string_view typeName(nlohmann::json::value_t t) {
            switch (t) {
            case nlohmann::json::value_t::string:
                return "string";
            case nlohmann::json::value_t::number_float:
            case nlohmann::json::value_t::number_integer:
            case nlohmann::json::value_t::number_unsigned:
                return "number";
            case nlohmann::json::value_t::array:
                return "array";
            case nlohmann::json::value_t::object:
                return "object";
            case nlohmann::json::value_t::boolean:
                return "boolean";
            case nlohmann::json::value_t::null:
                return "null";
            case nlohmann::json::value_t::binary:
                return "binary";
            case nlohmann::json::value_t::discarded:
                return "discarded";
            default:
                return "unknown";
            }
        }
    };

} // namespace core::utils::json

#ifdef _MSC_VER
#pragma warning(pop)
#endif