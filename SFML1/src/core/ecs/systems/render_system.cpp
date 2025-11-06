#include "core/ecs/systems/render_system.h"

namespace core::ecs {

    void RenderSystem::render(World& world, sf::RenderWindow& window) {

        // Берём хранилища

        auto& transforms = world.storage<TransformComponent>();
        auto& sprites    = world.storage<SpriteComponent>();

        // Итерируемся по ВСЕМ SpriteComponent,
        // хранящимся в ComponentStorage<SpriteComponent> (внутри это unordered_map<Entity, SpriteComponent>)
        // (можно оптимизировать: итерироваться по меньшему из двух storage)
        for (auto& [entity, spriteComp] : sprites) {
            if (auto* tr = transforms.get(entity)) { // есть ли трансформ у этой же сущности? 
                spriteComp.sprite.setPosition(tr->position); // если у сущности есть трансформ —
                                                             // обновляем позицию спрайта из TransformComponent
                                                             // tr->position — это sf::Vector2f,
                                                             // который хранит координаты сущности в мире.
                                                             // Таким образом, мы «прикрепляем» отрисовку
                                                             // к текущему положению сущности
            }
            // Рисуем спрайт в окне SFML
            window.draw(spriteComp.sprite);
        }
    }

} // namespace core::ecs