// ================================================================================================
// File: core/utils/json/json_validator.h
// Purpose: Lightweight schema-like validation helper for JSON configs.
// Used by: json_utils::parseAndValidateCritical/NonCritical, config loaders.
// Notes:
//  - Pure structural validation: no logging, no UI, no engine-level dependencies.
//  - On violation throws std::runtime_error with human-readable description.
// ================================================================================================
#pragma once

#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "third_party/json/json_silent.hpp"

#ifdef _MSC_VER
// C5045: "Spectre mitigation" шум при проходе по вектору правил.
// JsonValidator — чисто инфраструктурный слой, поэтому глушим ворнинг локально.
#pragma warning(push)
#pragma warning(disable : 5045)
#endif

namespace core::utils::json {

    /**
     * @brief Утилита для проверки корректности JSON-конфигурации.
     *
     * Проверяет, что ключи присутствуют и имеют допустимые типы данных.
     * Поддерживает несколько возможных типов для одного ключа.
     * Не занимается логированием/показом сообщений — только выбрасывает исключения.
     */
    class JsonValidator {
      public:
        using json = nlohmann::json;

        struct KeyRule {
            std::string name;
            std::vector<json::value_t> allowedTypes;
            bool required = true;
        };

        /**
         * @brief Проверяет, что JSON содержит все обязательные ключи и правильные типы.
         * @param data  JSON-объект (конфиг)
         * @param rules Список правил (std::span позволяет передавать vector/array без копирования).
         * @throws std::runtime_error при нарушении хотя бы одного правила или если root не объект.
         */
        static void validate(const json& data, std::span<const KeyRule> rules) {
            // Защита от передачи массива/null/примитива вместо конфига.
            if (!data.is_object()) {
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
                    expected.reserve(rule.allowedTypes.size() * 8); // small optimization

                    for (std::size_t i = 0; i < rule.allowedTypes.size(); ++i) {
                        // Используем string_view без лишних аллокаций
                        expected += typeName(rule.allowedTypes[i]);
                        if (i + 1 < rule.allowedTypes.size()) {
                            expected += " or ";
                        }
                    }

                    // std::string(typeName(type)) нужен для конкатенации
                    // со строковыми литералами в C++20
                    throw std::runtime_error("\nConfig validation error: key '" + rule.name +
                                             "' has wrong type (" + std::string(typeName(type)) +
                                             "), expected " + expected);
                }
            }
        }

      private:
        // Возвращаем string_view, так как литералы живут вечно (static storage).
        // Это Zero-overhead по сравнению с возвратом std::string.
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