#pragma once

#include <string>
#include <SFML/System/Vector2.hpp>

namespace core {

struct PlayerConfig {
    std::string texture = "assets/images/player.png";
    sf::Vector2f scale{1.f, 1.f};
    float speed = 100.f;
    sf::Vector2f startPosition{ 100.f, 100.f };
};

class ConfigLoader {
public:
    // Загружает JSON из файла и парсит PlayerConfig
    static PlayerConfig loadPlayerConfig(const std::string& path);
};    

} // namespace core