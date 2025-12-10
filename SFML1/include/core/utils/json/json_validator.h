// ================================================================================================
// File: core/utils/json/json_validator.h
// Purpose: Lightweight schema-like validation helper for JSON configs.
// Used by: json_utils::parseAndValidateCritical/NonCritical, config loaders, debug blueprints.
// Notes:
//  - Pure structural validation: no logging, no UI, no engine-level dependencies.
//  - On violation throws std::runtime_error with human-readable description.
// ================================================================================================
#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "third_party/json_silent.hpp"

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
         * @param rules Список правил для ключей.
         * @throws std::runtime_error при нарушении хотя бы одного правила.
         */
        static void validate(const json& data, const std::vector<KeyRule>& rules) {
            for (const auto& rule : rules) {
                const auto it = data.find(rule.name);
                if (it == data.end()) {
                    if (rule.required) {
                        throw std::runtime_error(
                            "\nConfig validation error: missing key '" + rule.name + "'"
                        );
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
                    expected.reserve(rule.allowedTypes.size() * 8); // маленькая микро-оптимизация

                    for (std::size_t i = 0; i < rule.allowedTypes.size(); ++i) {
                        expected += typeName(rule.allowedTypes[i]);
                        if (i + 1 < rule.allowedTypes.size()) {
                            expected += " or ";
                        }
                    }

                    throw std::runtime_error(
                        "\nConfig validation error: key '" + rule.name +
                        "' has wrong type (" + typeName(type) +
                        "), expected " + expected
                    );
                }
            }
        }

      private:
        static std::string typeName(nlohmann::json::value_t t) {
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