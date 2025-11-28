// ================================================================================================
// File: core/ecs/systems/render_system.h
// Purpose: Render sprites for entities that have TransformComponent + SpriteComponent
// Used by: World/SystemManager, high-level Game render loop
// Related headers: render_system.cpp, transform_component.h, sprite_component.h
// ================================================================================================
#pragma once

#include <SFML/Graphics.hpp>

#include "core/ecs/components/sprite_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/system.h"
#include "core/ecs/world.h"

namespace core::ecs {
    /**
    * @brief Система отрисовки.
    *
    * Ответственность:
    *  - ищет все сущности, у которых есть SpriteComponent;
    *  - при наличии TransformComponent копирует world-space позицию в sf::Sprite;
    *  - рисует спрайт в переданном sf::RenderWindow.
    *
    * Важно:
    *  - система не владеет ни окнами, ни ресурсами;
    *  - порядок отрисовки и z-order сейчас полностью определяются порядком обхода storage
    *    и самим sf::RenderWindow (без слоёв и сортировки);
    *  - позже можно добавить слои/батчинг/куллинг, не меняя базовый контракт:
    *      сфокусироваться на связке Transform + SpriteComponent.
    */
    class RenderSystem final : public ISystem {
      public:
        RenderSystem() = default;
        ~RenderSystem() override = default;

        void render(World& world, sf::RenderWindow& window) override;
    };

} // namespace core::ecs