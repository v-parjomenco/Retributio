// ================================================================================================
// File: core/ecs/systems/lock_system.h
// Purpose: Apply per-entity lock behavior on window/view resize
// Used by: Game resize handling, World/SystemManager
// Related headers: lock_behavior_component.h, transform_component.h, sprite_component.h
// ================================================================================================
#pragma once

#include "core/ecs/components/lock_behavior_component.h"
#include "core/ecs/components/sprite_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/system.h"
#include "core/ui/lock_behavior.h"

namespace core::ecs {

    /**
     * @brief Система фиксации сущностей при изменении размера окна/view.
     *
     * Поток данных:
     *  - Game при событии resize вызывает onResize(...);
     *  - система проходит по LockBehaviorComponent;
     *  - для сущностей с SpriteComponent применяет core::ui::applyScreenLock(...)
     *    или оставляет позицию в мировых координатах (World);
     *  - синхронизирует TransformComponent с позициями спрайтов, чтобы RenderSystem
     *    всегда рисовал актуальные world-space координаты.
     *
     * Важно:
     *  - onResize вызывается РЕДКО (при изменении размера окна), а не каждый кадр;
     *  - компоненты содержат только данные, логика живёт в core::ui;
     *  - система не знает ничего о конкретной игре и может использоваться
     *    в любых проектах поверх движка.
     */
    class LockSystem final : public ISystem {
      public:
        void onResize(World& world, const sf::View& view) {
            auto& locks         = world.storage<LockBehaviorComponent>();
            auto& sprites       = world.storage<SpriteComponent>();
            auto& transforms    = world.storage<TransformComponent>();

            for (auto& [entity, lockComp] : locks) {
                if (auto* sprite = sprites.get(entity)) {
                    switch (lockComp.kind) {
                    case core::ui::LockBehaviorKind::Screen:
                        core::ui::applyScreenLock(sprite->sprite, view, lockComp.previousViewSize,
                                                  lockComp.initialized);
                        break;
                    case core::ui::LockBehaviorKind::World:
                        // Живём в мировых координатах, позицию не трогаем.
                        break;
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