// ================================================================================================
// File: core/ecs/systems/lock_system.h
// Purpose: Apply per-entity lock behavior on window/view resize (EnTT backend)
// Used by: Game resize handling, World/SystemManager
// Related headers: lock_behavior_component.h, transform_component.h
// ================================================================================================
#pragma once

#include <SFML/Graphics/View.hpp>
#include <SFML/System/Vector2.hpp>

#include "core/ecs/components/lock_behavior_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/system.h"
#include "core/ecs/world.h"
#include "core/ui/lock_behavior.h"

namespace sf {
    class RenderWindow;
}

namespace core::ecs {

    /**
     * @brief Система фиксации сущностей при изменении размера окна/view.
     *
     * Политика:
     *  - newViewSize может быть 0/NaN при сворачивании/краевых состояниях окна — это штатно.
     *    В таком случае делаем ранний выход (без прохода по ECS) и не трогаем previousViewSize.
     *
     * Validate on write, trust on read:
     *  - newViewSize проверяется здесь (runtime guard для OS-данных);
     *  - previousViewSize защищён в computeScreenLockPosition (fallback при invalid);
     *  - newPosition ГАРАНТИРОВАННО finite и valid → проверки НЕТ.
     */
    class LockSystem final : public ISystem {
      public:
        void onResize(World& world, const sf::View& view) {
            const sf::Vector2f newViewSize = view.getSize();
            constexpr float kMinViewComponent = 1e-3f;

            // newViewSize — данные от OS. 0/NaN возможны (minimized).
            // Не тратим CPU на проход по ECS.
            if (!(newViewSize.x > kMinViewComponent) || !(newViewSize.y > kMinViewComponent)) {
                return;
            }

            auto v = world.view<LockBehaviorComponent, TransformComponent>();

            for (auto [entity, lockComp, transform] : v.each()) {
                (void)entity;

                if (lockComp.kind != core::ui::LockBehaviorKind::Screen) {
                    continue;
                }

                const sf::Vector2f newPosition = core::ui::computeScreenLockPosition(
                    transform.position,
                    lockComp.previousViewSize,
                    newViewSize);

                transform.position = newPosition;

                // Обновляем только после успешного применения (и только при валидном newViewSize).
                lockComp.previousViewSize = newViewSize;
            }
        }

        void update(World&, float) override {
        }

        void render(World&, sf::RenderWindow&) override {
        }
    };

} // namespace core::ecs