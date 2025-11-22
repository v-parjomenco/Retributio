// ================================================================================================
// File: core/config/properties/sprite_properties.h
// Purpose: Data-only sprite configuration brick (texture id + scale)
// Used by: PlayerBlueprint, future entity blueprints
// Related headers: core/resources/ids/resource_ids.h
// ================================================================================================
#pragma once

#include <SFML/System/Vector2.hpp>

#include "core/resources/ids/resource_ids.h"

namespace core::config::properties {

    /**
 * @brief Набор данных для описания спрайта сущности.
 *
 * Эта структура ничего не знает о том, КТО именно будет пользоваться спрайтом
 * (игрок, враг, элемент интерфейса и т.п.).
 * Она описывает только связь с текстурой и базовый масштаб.
 */
    struct SpriteProperties {
        /**
     * @brief Идентификатор текстуры.
     *
     * Здесь хранится только логический ID (enum TextureID), а не путь к файлу.
     * Реальный путь вытаскивается через ResourcePaths + ResourceManager.
     *
     * Сейчас по умолчанию используется TextureID::Player, так как первый клиент —
     * конфигурация игрока. Позже можно будет задавать общий дефолт или вообще
     * требовать явного указания ID в JSON.
     */
        core::resources::ids::TextureID textureId = core::resources::ids::TextureID::Player;

        /**
     * @brief Базовый масштаб спрайта по осям X и Y.
     *
     * Значения 1.0f означают «без дополнительного масштабирования».
     * Конкретные игры могут переопределять это значение через JSON.
     */
        sf::Vector2f scale{1.f, 1.f};
    };

} // namespace core::config::properties