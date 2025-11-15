#pragma once

#include "core/ecs/components/scaling_behavior_component.h"
#include "core/ecs/components/sprite_component.h"
#include "core/ecs/system.h"

namespace core::ecs {

    /**
     * @brief Система масштабирования на событие ресайза
     * Применяет UniformScalingPolicy только для сущностей с mode == Uniform.
     */
    class ScalingSystem final : public ISystem {
      public:
        void onResize(World& world, const sf::View& newView) {
            auto& sprites = world.storage<SpriteComponent>();
            auto& scalings = world.storage<ScalingBehaviorComponent>();

            for (auto& [entity, sc] : scalings) {
                if (sc.mode == ScalingBehaviorComponent::Mode::Uniform) {
                    if (auto* sp = sprites.get(entity)) {
                        sc.policy.apply(sp->sprite, newView);
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