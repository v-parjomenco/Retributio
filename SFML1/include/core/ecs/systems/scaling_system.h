#pragma once

#include "core/ecs/components/scaling_behavior_component.h"
#include "core/ecs/components/sprite_component.h"
#include "core/ecs/system.h"
#include "core/ui/scaling_behavior.h"

namespace core::ecs {

    /**
     * @brief Система масштабирования на событие ресайза.
     *
     * Применяет поведение Uniform только для сущностей,
     * у которых kind == ScalingBehaviorKind::Uniform.
     */
    class ScalingSystem final : public ISystem {
      public:
        void onResize(World& world, const sf::View& newView) {
            auto& sprites = world.storage<SpriteComponent>();
            auto& scalings = world.storage<ScalingBehaviorComponent>();

            for (auto& [entity, sc] : scalings) {
                if (auto* sp = sprites.get(entity)) {
                    switch (sc.kind) {
                    case core::ui::ScalingBehaviorKind::Uniform:
                        core::ui::applyUniformScaling(sp->sprite, newView, sc.lastUniform);
                        break;
                    case core::ui::ScalingBehaviorKind::None:
                        // Ничего не делаем.
                        break;
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