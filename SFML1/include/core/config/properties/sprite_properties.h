// ================================================================================================
// File: core/config/properties/sprite_properties.h
// Purpose: Data-only sprite configuration brick (texture key + scale)
// Used by: PlayerBlueprint, future entity blueprints
// Related headers: core/resources/keys/resource_key.h
// ================================================================================================
#pragma once

#include <SFML/System/Vector2.hpp>

#include "core/resources/keys/resource_key.h"

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
         * @brief Ключ текстуры.
         *
         * Здесь хранится RuntimeKey32 текстуры, а не путь к файлу.
         * Реальный путь вытаскивается через ResourceRegistry + ResourceManager.
         *
         * Ключ задаётся на этапе загрузки конфига (validate-on-write).
         */
        core::resources::TextureKey texture{};

        /**
         * @brief Масштаб спрайта по осям X и Y.
         *
         * Значение 1.0f означает «без дополнительного масштабирования».
         * Конкретные игры моугут переопределять это значение через конфиг или JSON.
         */
        sf::Vector2f scale{1.f, 1.f};
    };

} // namespace core::config::properties