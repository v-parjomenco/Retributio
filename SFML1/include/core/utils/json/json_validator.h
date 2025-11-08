#pragma once

#include <string>
#include <vector>
#include <stdexcept>
#include <third_party/json_silent.hpp>
#include "core/utils/message.h"

namespace core::utils::json {

    /**
     * @brief Утилита для проверки корректности JSON-конфигурации игрока.
     *
     * Проверяет, что ключи присутствуют и имеют допустимые типы данных.
     * Поддерживает несколько возможных типов для одного ключа.
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
         * @param data - JSON объект (player.json)
         * @param rules - список правил
         * @throws std::runtime_error при нарушении
         */
        static void validate(const json& data, const std::vector<KeyRule>& rules) {
            for (const auto& rule : rules) {

// Нужно включить когда будет запись логов, а не всплывающие сообщения
//#ifdef _DEBUG
//                DEBUG_MSG("[JsonValidator]\nПроверка ключа: " + rule.name);
//#endif

                if (!data.contains(rule.name)) {
                    if (rule.required)
                        throw std::runtime_error("\nConfig validation error: missing key '" + rule.name + "'");
                    else
                        continue;
                }

                const auto& value = data.at(rule.name);
                const auto type = value.type();

                bool valid = false;
                for (auto allowed : rule.allowedTypes) {
                    if (type == allowed) {
                        valid = true;
                        break;
                    }
                }

                if (!valid) {
                    std::string expected;
                    for (size_t i = 0; i < rule.allowedTypes.size(); ++i) {
                        expected += typeName(rule.allowedTypes[i]);
                        if (i + 1 < rule.allowedTypes.size()) expected += " or ";
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
            case nlohmann::json::value_t::string: return "string";
            case nlohmann::json::value_t::number_float:
            case nlohmann::json::value_t::number_integer:
            case nlohmann::json::value_t::number_unsigned: return "number";
            case nlohmann::json::value_t::array: return "array";
            case nlohmann::json::value_t::object: return "object";
            case nlohmann::json::value_t::boolean: return "boolean";
            case nlohmann::json::value_t::null: return "null";
            case nlohmann::json::value_t::binary: return "binary";
            case nlohmann::json::value_t::discarded: return "discarded";
            default: return "unknown";
            }
        }
    };

} // namespace core::utils::json