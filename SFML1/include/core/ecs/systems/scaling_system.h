// ================================================================================================
// File: core/ecs/systems/scaling_system.h
// Purpose: Apply per-entity scaling behavior on window/view resize (deterministic, absolute-based)
// Used by: Optional resize handling
// Related headers: scaling_behavior_component.h, sprite_component.h, 
//                  sprite_scaling_data_component.h, core/ui/scaling_behavior.h
// Notes:
//  - Not used by SkyGuard. Kept for potential usage in future games/tools.
// ================================================================================================
#pragma once
#include <SFML/Graphics/View.hpp>
#include <SFML/System/Vector2.hpp>

#include "core/ecs/components/scaling_behavior_component.h"
#include "core/ecs/components/sprite_component.h"
#include "core/ecs/components/sprite_scaling_data_component.h"
#include "core/ecs/system.h"
#include "core/ecs/world.h"
#include "core/ui/scaling_behavior.h"

namespace sf {
    class RenderWindow;
}

namespace core::ecs {
    /**
     * @brief Система масштабирования сущностей при resize.
     *
     * Детерминированная формула:
     *   scale = baseScale * uniformFactor
     *
     * Принципиально: не накапливаем float-ошибки,
     * т.к. каждый resize пересчитывает scale от baseScale.
     *
     * HOT/COLD SPLIT:
     *  - baseScale (cold) → SpriteScalingDataComponent (читается только здесь)
     *  - scale (hot) → SpriteComponent (пишется здесь, читается в RenderSystem)
     *
     * Validate on write, trust on read:
     *  - baseScale валидируется в config_loader (> 0);
     *  - baseViewSize валидируется на границе записи (лоадер/инициализация компонента) (> 0);
     *  - currentViewSize защищён в computeUniformFactor (fallback на 1.0 при invalid);
     *  - newScale предполагается корректным при условии валидных входных данных (finite, > 0).
     */
    class ScalingSystem final : public ISystem {
      public:
        void onResize(World& world, const sf::View& newView) noexcept {
            auto view = world.view<ScalingBehaviorComponent, 
                                   SpriteComponent, 
                                   SpriteScalingDataComponent>();

            // Loop-invariant: размер view одинаков для всех сущностей в рамках одного resize.
            const sf::Vector2f currentViewSize = newView.getSize();

            for (auto [entity, scaling, sprite, scalingData] : view.each()) {
                (void)entity;

                switch (scaling.kind) {
                case core::ui::ScalingBehaviorKind::Uniform: {
                    const float newUniform =
                        core::ui::computeUniformFactor(currentViewSize, scaling.baseViewSize);

                    const sf::Vector2f newScale{
                        scalingData.baseScale.x * newUniform,
                        scalingData.baseScale.y * newUniform
                    };

                    sprite.scale = newScale;
                    scaling.lastUniform = newUniform;
                    break;
                }
                case core::ui::ScalingBehaviorKind::None:
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