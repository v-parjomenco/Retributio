// ================================================================================================
// File: core/ecs/components/sprite_component.h
// Purpose: Per-entity drawable sprite for RenderSystem
// Used by: RenderSystem, game-specific init systems
// Related headers: transform_component.h, render_system.h
// ================================================================================================
#pragma once

#include <SFML/Graphics.hpp>
#include <utility> // для std::move

namespace core::ecs {

    /**
     * @brief Компонент для отрисовки сущности.
     *
     * Хранит sf::Sprite напрямую, чтобы RenderSystem мог:
     *  - обновить его позицию из TransformComponent;
     *  - нарисовать в sf::RenderWindow.
     *
     * Особенности SFML 3:
     *  - sf::Sprite не имеет конструктора по умолчанию,
     *    поэтому компонент можно создать только явно (см. explicit конструктор).
     *
     * План на будущее (resource-driven):
     *  - заменить хранение "живого" sf::Sprite на лёгкую модель:
     *      SpriteComponent { TextureID, IntRect, zOrder }
     *  - RenderSystem будет брать текстуру из ResourceManager по TextureID;
     *  - это уменьшит размер компонента и упростит стриминг ресурсов
     *    на больших картах и при множестве сущностей.
     *
     * Сейчас такой "прямой" sf::Sprite — нормальный компромисс для прототипов
     * и небольших игр (SkyGuard, платформеры), но архитектура уже предполагает
     * переход к ID-based ресурсоёмкой модели.
     */
    struct SpriteComponent {
        sf::Sprite sprite;

        // Явный конструктор — обязателен, т.к. sf::Sprite() = delete
        explicit SpriteComponent(sf::Sprite spr) : sprite(std::move(spr)) {
        }

        // Удаляем дефолтный конструктор, т.к. у sf::Sprite его нет
        SpriteComponent() = delete;
    };

} // namespace core::ecs