#pragma once

#include <string>
#include <SFML/System/Vector2.hpp>
#include "core/resources/resourceIDs.h"

namespace core {

struct PlayerConfig {
    std::string texturePath = "assets/images/player4.png"; // путь к файлу
    sf::Vector2f scale{1.f, 1.f};
    float speed = 100.f;

    std::string anchor = "BottomCenter"; // базовый якорь по умолчанию, используется для задачи стартовой позиции
    sf::Vector2f startPosition{ 0.f, 0.f }; // позиция по умолчанию, используется только если якорь не задан

    std::string resizeBehavior = "FixedWorldNoScaling"; // поведение по умолчанию при изменении размера окна
};

class ConfigLoader {
public:
    // Загружает JSON из файла и парсит PlayerConfig
    static PlayerConfig loadPlayerConfig(const std::string& path);
};    

} // namespace core