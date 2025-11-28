// ================================================================================================
// File: core/ecs/systems/scaling_system.h
// Purpose: Apply per-entity scaling behavior on window/view resize
// Used by: Game resize handling, World/SystemManager
// Related headers: scaling_behavior_component.h, sprite_component.h
// ================================================================================================
#pragma once

#include "core/ecs/components/scaling_behavior_component.h"
#include "core/ecs/components/sprite_component.h"
#include "core/ecs/system.h"
#include "core/ui/scaling_behavior.h"

namespace core::ecs {

    /**
     * @brief Система масштабирования сущностей при ресайзе окна/view.
     *
     * Поток данных:
     *  - Game при событии resize вызывает onResize(...);
     *  - система проходит по ScalingBehaviorComponent;
     *  - для сущностей с kind == ScalingBehaviorKind::Uniform вызывает
     *    core::ui::applyUniformScaling(...) и обновляет sprite;
     *  - для kind == None ничего не делает (оставляет размер как есть).
     *
     * Важно:
     *  - onResize вызывается РЕДКО, а не каждый кадр;
     *  - логика масштабирования инкапсулирована в core::ui, система лишь
     *    связывает компоненты ECS с UI-слоем;
     *  - поведение полностью data-driven: значения kind/baseViewSize/lastUniform
     *    задаются в конфиге игры.
     */
    class ScalingSystem final : public ISystem {
      public:
        void onResize(World& world, const sf::View& newView) {
            auto& sprites   = world.storage<SpriteComponent>();
            auto& scalings  = world.storage<ScalingBehaviorComponent>();

            for (auto& [entity, sc] : scalings) {
                if (auto* sp = sprites.get(entity)) {
                    switch (sc.kind) {
                    case core::ui::ScalingBehaviorKind::Uniform:
                        core::ui::applyUniformScaling(sp->sprite, newView, sc.baseViewSize,
                                                      sc.lastUniform);
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