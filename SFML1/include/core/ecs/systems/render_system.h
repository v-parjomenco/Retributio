#pragma once

#include <SFML/Graphics.hpp>

#include "core/ecs/components/transform_component.h"
#include "core/ecs/components/sprite_component.h"
#include "core/ecs/system.h"
#include "core/ecs/world.h"

namespace core::ecs {
    /**
    * @brief Система отрисовки
    * - ищет все сущности, у которых есть и TransformComponent, и SpriteComponent
    * - переносит позицию в спрайт
    * - рисует спрайт
    */
    class RenderSystem final : public ISystem {
    public:
        RenderSystem() = default;
        ~RenderSystem() override = default;

        void render(World& world, sf::RenderWindow& window) override;
    };

} // namespace core::ecs