// ================================================================================================
// File: core/utils/json/json_accessors.h
// Purpose: Convenience JSON accessors for loaders (parseValue/getOptional/parseEnum).
// Used by: Config loaders and other mapping code.
// ================================================================================================
#pragma once

#include <optional>
#include <string>
#include <string_view>

#include <SFML/System/Vector2.hpp>

#include "core/utils/json/json_common.h"
#include "third_party/json/json_silent.hpp"

namespace core::utils::json {

    /**
     * @brief Универсальный шаблон для чтения значения из JSON.
     *
     * Если ключ присутствует и тип совместим, берём T из JSON.
     * Если ключ отсутствует или чтение/конвертация не удалась — возвращаем defaultValue.
     */
    template <typename T>
    T parseValue(const json& data, std::string_view key, const T& defaultValue) {
        const auto it = data.find(key);
        if (it == data.end()) {
            return defaultValue;
        }

        try {
            return it->get<T>();
        } catch (const json::exception&) {
            return defaultValue;
        }
    }

    // --------------------------------------------------------------------------------------------
    // Специализации parseValue<T>
    // --------------------------------------------------------------------------------------------

    template <>
    float parseValue<float>(const json& data,
                            std::string_view key,
                            const float& defaultValue);

    template <>
    std::string parseValue<std::string>(const json& data,
                                        std::string_view key,
                                        const std::string& defaultValue);

    template <>
    sf::Vector2f parseValue<sf::Vector2f>(const json& data,
                                          std::string_view key,
                                          const sf::Vector2f& defaultValue);

    template <>
    bool parseValue<bool>(const json& data,
                          std::string_view key,
                          const bool& defaultValue);

    template <>
    unsigned parseValue<unsigned>(const json& data,
                                  std::string_view key,
                                  const unsigned& defaultValue);

    // --------------------------------------------------------------------------------------------
    // Enum parser
    // --------------------------------------------------------------------------------------------

    template <typename Enum, typename Mapper, typename ErrorHandler>
    Enum parseEnum(const json& data,
                   const char* key,
                   Enum defaultValue,
                   Mapper mapper,
                   ErrorHandler onInvalidValue) {
        const auto it = data.find(key);
        if (it == data.end()) {
            return defaultValue;
        }

        const auto* namePtr = it->get_ptr<const std::string*>();
        if (namePtr == nullptr) {
            return defaultValue;
        }

        const std::string_view name{*namePtr};

        if (auto idOpt = mapper(name)) {
            return *idOpt;
        }

        onInvalidValue(name);
        return defaultValue;
    }

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

    // --------------------------------------------------------------------------------------------
    // Optional
    // --------------------------------------------------------------------------------------------

    template <typename T>
    std::optional<T> getOptional(const json& data, const char* key) {
        const auto it = data.find(key);
        if (it == data.end()) {
            return std::nullopt;
        }

        try {
            return it->get<T>();
        } catch (const json::exception&) {
            return std::nullopt;
        }
    }

    // Запрещаем вызовы с временным json: это потенциально приводит к dangling view
    // (и в целом не имеет смысла в нашей архитектуре загрузчиков).
    template <typename T> std::optional<T>
    getOptional(const json&&, const char*) = delete;
    // (удалённая template-перегрузка выше покрывает и string_view)

    template <>
    inline std::optional<std::string_view> getOptional<std::string_view>(const json& data,
                                                                         const char* key) {
        const auto it = data.find(key);
        if (it == data.end()) {
            return std::nullopt;
        }

        const auto* ptr = it->get_ptr<const std::string*>();
        if (ptr == nullptr) {
            return std::nullopt;
        }

        return std::string_view{*ptr};
    }

} // namespace core::utils::json