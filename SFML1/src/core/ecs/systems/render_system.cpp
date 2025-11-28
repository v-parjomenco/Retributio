#include "pch.h"

#include "core/ecs/systems/render_system.h"

namespace core::ecs {

    void RenderSystem::render(World& world, sf::RenderWindow& window) {

        // Берём хранилища

        auto& transforms = world.storage<TransformComponent>();
        auto& sprites = world.storage<SpriteComponent>();

        // Идём по всем сущностям, у которых есть SpriteComponent.
        // Для каждой:
        //  - если есть TransformComponent — переносим world-space позицию в sf::Sprite;
        //  - затем рисуем спрайт (даже если Transform отсутствует, тогда позиция остаётся той,
        //    что была до этого).
        //
        // Сложность — O(|sprites|). Для крупных игр это важно: количество спрайтов обычно меньше,
        // чем всех сущностей с Transform (логика, физика и т.п.).
        for (auto& [entity, spriteComp] : sprites) {
            if (auto* tr = transforms.get(entity)) { // есть ли трансформ у этой же сущности?
                // Если у сущности есть трансформ — обновляем позицию спрайта из TransformComponent
                // tr->position — это sf::Vector2f, который хранит координаты сущности в мире.
                // Таким образом, мы «прикрепляем» отрисовку к текущему положению сущности.
                spriteComp.sprite.setPosition(tr->position);
            }
            window.draw(spriteComp.sprite);
        }
    }

} // namespace core::ecs