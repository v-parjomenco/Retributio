#pragma once

#include <string>

#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Keyboard.hpp>

#include "core/config.h"
#include "core/resources/ids/resourceIDs.h"
#include "core/ui/anchor_utils.h"

namespace core::config {

    // Задаёт поведение по умолчанию, если не загрузилась конфигурация из JSON
struct PlayerConfig {
    std::string texturePath = ::config::PLAYER_TEXTURE;
    sf::Vector2f scale{1.f, 1.f};
    float speed = ::config::PLAYER_SPEED;
    float acceleration = ::config::PLAYER_ACCELERATION;
    float friction = ::config::PLAYER_FRICTION;

    // Установка параметров по умолчанию
    std::string   anchor = ::config::DEFAULT_ANCHOR;    // якорь по умолчанию
    sf::Vector2f startPosition{ 0.f, 0.f };             // стартовая позиция, если якорь не задан
    std::string   scaling = ::config::DEFAULT_SCALING;  // поведение при изменении размера экрана
    std::string   lockBehavior = ::config::DEFAULT_LOCK_BEHAVIOR; // фиксация по умолчанию

    // Управление с клавиатуры (можно переопределить в JSON)
    struct Controls {
        sf::Keyboard::Key up = ::config::DEFAULT_KEY_UP;
        sf::Keyboard::Key down = ::config::DEFAULT_KEY_DOWN;
        sf::Keyboard::Key left = ::config::DEFAULT_KEY_LEFT;
        sf::Keyboard::Key right = ::config::DEFAULT_KEY_RIGHT;
    } controls;
};

class ConfigLoader {
public:
    // Загружает JSON из файла и парсит PlayerConfig
    static PlayerConfig loadPlayerConfig(const std::string& path);
};    

} // namespace config