// ================================================================================================
// File: core/config/config_loader.h
// Purpose: High-level player configuration loader (JSON -> PlayerConfig)
// Used by: Game, PlayerInitSystem
// Related headers: core/config/config_keys.h, core/utils/json/json_utils.h,
//                  core/utils/json/json_validator.h, core/utils/file_loader.h
// ================================================================================================
#pragma once

#include <string>

#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Keyboard.hpp>

#include "core/config.h"
#include "core/resources/ids/resourceIDs.h"
#include "core/ui/anchor_utils.h"

namespace core::config {

    // Высокоуровневая, уже прошедшая парсинг конфигурация для сущности игрока
    // JSON-данные лишь заполняют эту структуру; значения по умолчанию берутся из config.h.
    struct PlayerConfig {
        // Базовый путь к текстуре игрока
        std::string texturePath = ::config::PLAYER_TEXTURE;
        // Коэффициент масштабирования спрайта игрока
        sf::Vector2f scale{1.f, 1.f};
        // Параметры движения игрока
        float speed = ::config::PLAYER_SPEED;
        float acceleration = ::config::PLAYER_ACCELERATION;
        float friction = ::config::PLAYER_FRICTION;

        // Параметры привязки и поведения камеры/экрана
        std::string anchor = ::config::DEFAULT_ANCHOR;   // якорь по умолчанию
        sf::Vector2f startPosition{0.f, 0.f};            // стартовая позиция, если якорь не задан
        std::string scaling = ::config::DEFAULT_SCALING; // поведение при изменении размера экрана
        std::string lockBehavior = ::config::DEFAULT_LOCK_BEHAVIOR; // фиксация по умолчанию

        // Управление с клавиатуры (может переопределяться в JSON)
        struct Controls {
            sf::Keyboard::Key up = ::config::DEFAULT_KEY_UP;
            sf::Keyboard::Key down = ::config::DEFAULT_KEY_DOWN;
            sf::Keyboard::Key left = ::config::DEFAULT_KEY_LEFT;
            sf::Keyboard::Key right = ::config::DEFAULT_KEY_RIGHT;
        } controls;
    };

    // Высокоуровневый загрузчик, преобразующий информацию в JSON-файле в PlayerConfig.
    // 
    // Обязанности:
    //  - Использует FileLoader, чтобы прочесть JSON-файл как текст.
    //  - Строит дерево Json из текста с помощью json::parse от nlohmann.
    //  - С помощью JsonValidator валидирует структуру JSON-файла.
    //  - Превращает поля JSON-файла в поля конфигурации игрока PlayerConfig с помощью json_utils.
    //
    // Этот загрузчик НЕ:
    //  - имеет дел с низкоуровневой загрузкой (ей занимается FileLoader),
    //  - знает ничего о рендеринге, ECS и прочем.
    class ConfigLoader {
      public:
        // Загружает JSON из файла и парсит данные в PlayerConfig
        //
        // Возможное поведение:
        //  - Если файл не может быть прочитан
        //      -> showError(...) + возврат значения PlayerConfig по умолчанию.
        //  - Если информация в JSON невалидна или искажена 
        //      -> showError(...) + std::exit(EXIT_FAILURE).
        //  - Если структура JSON неверна
        //      -> showError(...) + std::exit(EXIT_FAILURE).
        //
        static PlayerConfig loadPlayerConfig(const std::string& path);
    };

} // namespace core::config