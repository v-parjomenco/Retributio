#include "pch.h"

#include "core/utils/json/json_accessors.h"

#include "core/utils/json/json_parsers.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 5045)
#endif

namespace core::utils::json {

    template <>
    float parseValue<float>(const json& data, std::string_view key, const float& defaultValue) {
        const auto it = data.find(key);
        if (it == data.end()) {
            return defaultValue;
        }

        const auto& v = *it;
        if (!v.is_number()) {
            return defaultValue;
        }

        float out = 0.f;
        return detail::tryGetFiniteFloat(v, out) ? out : defaultValue;
    }

    template <>
    std::string parseValue<std::string>(const json& data,
                                        std::string_view key,
                                        const std::string& defaultValue) {
        const auto it = data.find(key);
        if (it == data.end()) {
            return defaultValue;
        }

        const auto* ptr = it->get_ptr<const std::string*>();
        if (ptr == nullptr) {
            return defaultValue;
        }

        return *ptr; // copy; bad_alloc пусть пробрасывается (это не “валидируемая” ошибка конфига)
    }

    template <>
    sf::Vector2f parseValue<sf::Vector2f>(const json& data,
                                          std::string_view key,
                                          const sf::Vector2f& defaultValue) {
        return parseVec2fWithIssue(data, key, defaultValue).value;
    }

    template <>
    bool parseValue<bool>(const json& data, std::string_view key, const bool& defaultValue) {
        const auto it = data.find(key);
        if (it == data.end()) {
            return defaultValue;
        }

        const auto* ptr = it->get_ptr<const json::boolean_t*>();
        if (ptr == nullptr) {
            return defaultValue;
        }

        return *ptr;
    }

    template <>
    unsigned parseValue<unsigned>(const json& data,
                                  std::string_view key,
                                  const unsigned& defaultValue) {
        return parseUnsignedWithIssue(data, key, defaultValue).value;
    }

} // namespace core::utils::json

#ifdef _MSC_VER
#pragma warning(pop)
#endif