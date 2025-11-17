#pragma once

#include <string>

#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Keyboard.hpp>

#include "third_party/json_silent.hpp"

namespace core::utils::json {

    using json = nlohmann::json;

    // Универсальный шаблон для чтения значения из JSON
    template <typename T>
    T parseValue(const json& data, const std::string& key, const T& defaultValue);

    // Специализации — объявляются здесь, реализуются в .cpp
    template <>
    float parseValue<float>(const json& data, const std::string& key, const float& defaultValue);
    template <>
    std::string parseValue<std::string>(const json& data, const std::string& key,
                                        const std::string& defaultValue);
    template <>
    sf::Vector2f parseValue<sf::Vector2f>(const json& data, const std::string& key,
                                          const sf::Vector2f& defaultValue);

    sf::Keyboard::Key parseKey(const json& data, const std::string& key,
                               sf::Keyboard::Key defaultValue);

    sf::Color parseColor(const json& data, const std::string& key,
                         const sf::Color& defaultValue);

} // namespace core::utils::json