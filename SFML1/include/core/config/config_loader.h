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
#include "core/resources/ids/resource_ids.h"
#include "core/ui/anchor_utils.h"

namespace core::config {

    /**
     * @brief Высокоуровневая, уже прошедшая парсинг конфигурация для сущности игрока.
     *
     * JSON-данные лишь заполняют эту структуру; значения по умолчанию берутся из config.h
     * и/или из дефолтных значений полей.
     *
     * ВАЖНО:
     *  - Путь к файлу текстуры больше нигде не хранится в геймплейных конфигах;
     *    единственный источник путей — assets/config/resources.json (см. ResourcePaths).
     *  - Здесь мы храним только TextureID (например, TextureID::Player).
     */
    struct PlayerConfig {
        /**
        * @brief Идентификатор текстуры игрока (enum class TextureID), а не прямой путь к файлу.
        * 
        * Строка "texture" из player.json → маппится в TextureID (через textureFromString),
        * а реальный путь берётся из assets/config/resources.json через ResourcePaths.
        */
        core::resources::ids::TextureID textureId = core::resources::ids::TextureID::Player;
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
            sf::Keyboard::Key up    = ::config::DEFAULT_KEY_UP;
            sf::Keyboard::Key down  = ::config::DEFAULT_KEY_DOWN;
            sf::Keyboard::Key left  = ::config::DEFAULT_KEY_LEFT;
            sf::Keyboard::Key right = ::config::DEFAULT_KEY_RIGHT;
        } controls;
    };

    /**
     * @brief Высокоуровневый загрузчик, преобразующий JSON-файл в PlayerConfig.
     *
     * Обязанности:
     *  - Использует FileLoader, чтобы прочесть JSON-файл как текст.
     *  - Строит дерево Json из текста с помощью json::parse от nlohmann.
     *  - С помощью JsonValidator валидирует структуру JSON-файла (parseAndValidateCritical).
     *  - Превращает поля JSON-файла в поля PlayerConfig с помощью json_utils.
     *
     * НЕ:
     *  - занимается низкоуровневой загрузкой ресурсов (диском/Texture/Font),
     *  - знает что-либо о рендеринге, ECS и прочем.
     */
    class ConfigLoader {
      public:
        /**
         * @brief Загружает JSON из файла и парсит данные в PlayerConfig.
         *
         * Поведение:
         *  - Если файл не может быть прочитан:
         *      -> showError(...) + возврат значения PlayerConfig по умолчанию.
         *  - Если информация в JSON невалидна или искажена:
         *      -> showError(...) + std::exit(EXIT_FAILURE).
         *  - Если структура JSON неверна:
         *      -> showError(...) + std::exit(EXIT_FAILURE).
         */
        static PlayerConfig loadPlayerConfig(const std::string& path);
    };

} // namespace core::config
