// ================================================================================================
// File: core/ecs/systems/lock_system.h
// Purpose: Apply per-entity lock behavior on window/view resize (EnTT backend)
// Used by: Game resize handling, World/SystemManager
// Related headers: lock_behavior_component.h, transform_component.h, sprite_component.h
// ================================================================================================
#pragma once

#include <SFML/Graphics.hpp>

#include "core/ecs/components/lock_behavior_component.h"
#include "core/ecs/components/sprite_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/system.h"
#include "core/ecs/world.h"
#include "core/ui/lock_behavior.h"

namespace core::ecs {

    /**
     * @brief Система фиксации сущностей при изменении размера окна/view.
     *
     * Поток данных (EnTT backend):
     *  - Game при событии resize вызывает onResize(...);
     *  - система проходит по сущностям с LockBehaviorComponent + SpriteComponent + TransformComponent;
     *  - Screen: пересчитывает позицию через core::ui::computeScreenLockPosition(...),
     *    сохраняя относительное положение в долях от предыдущего view.
     *  - World: позицию не меняет.
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
            // EnTT view: только сущности со всеми тремя компонентами
            auto v = world.view<LockBehaviorComponent, SpriteComponent, TransformComponent>();

            const sf::Vector2f newViewSize = view.getSize();

            for (auto [entity, lockComp, sprite, transform] : v.each()) {
                (void) entity; // пока не нужен
                (void) sprite; // может понадобиться для bounds в будущем

                switch (lockComp.kind) {
                case core::ui::LockBehaviorKind::Screen: {

                transform.position = core::ui::computeScreenLockPosition(
                    transform.position,
                    lockComp.previousViewSize,
                    newViewSize);

                    // Обновляем сохранённый размер для следующего resize
                    lockComp.previousViewSize = newViewSize;
                    lockComp.initialized = true;

                    break;
                }

                case core::ui::LockBehaviorKind::World:
                    // Живём в мировых координатах, позицию не трогаем.
                    break;
                }
            }
        }

        void update(World&, float) override {
        }
        void render(World&, sf::RenderWindow&) override {
        }
    };

} // namespace core::ecs