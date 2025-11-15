#pragma once

#include "core/ecs/components/lock_behavior_component.h"
#include "core/ecs/components/sprite_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/system.h"

namespace core::ecs {

    class LockSystem final : public ISystem {
      public:
        void onResize(World& world, const sf::View& view) {
            auto& locks = world.storage<LockBehaviorComponent>();
            auto& sprites = world.storage<SpriteComponent>();
            auto& transforms = world.storage<TransformComponent>();

            for (auto& [entity, lockComp] : locks) {
                if (auto* sprite = sprites.get(entity)) {
                    // Применяем политику к спрайту
                    if (lockComp.policy) {
                        lockComp.policy->apply(sprite->sprite, view);
                    }
                    // Обязательно синхронизируем Transform, иначе RenderSystem перетрёт позицию
                    if (auto* tr = transforms.get(entity)) {
                        tr->position = sprite->sprite.getPosition();
                    }
                }
            }
        }

        void update(World&, float) override {
        }
        void render(World&, sf::RenderWindow&) override {
        }
    };

} // namespace core::ecs