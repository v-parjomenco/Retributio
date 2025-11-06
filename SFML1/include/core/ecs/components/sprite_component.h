#pragma once

#include <SFML/Graphics.hpp>

namespace core::ecs {

    /**
     * @brief Компонент для отрисовки.
     * Держит sf::Sprite прямо здесь, чтобы RenderSystem мог его нарисовать.
     * В SFML 3.0 sf::Sprite не имеет конструктора по умолчанию —
     * поэтому этот компонент можно создавать только явно.
     *
     * Позже можно заменить на:
     *  - SpriteComponent { TextureID, IntRect, zOrder }
     *  - и RenderSystem сам возьмёт текстуру из ResourceManager
     */
    struct SpriteComponent {
        sf::Sprite sprite;

        // Явный конструктор — обязателен, т.к. sf::Sprite() = delete
        explicit SpriteComponent(sf::Sprite spr)
            : sprite(std::move(spr)) {
        }

        // Удаляем дефолтный конструктор, т.к. у sf::Sprite его нет
        SpriteComponent() = delete;
    };

} // namespace core::ecs