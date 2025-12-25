// ================================================================================================
// File: core/ecs/systems/scaling_system.h
// Purpose: Apply per-entity scaling behavior on window/view resize (deterministic, absolute-based)
// Used by: Game resize handling, World/SystemManager
// Related headers: scaling_behavior_component.h, sprite_component.h, core/ui/scaling_behavior.h
// ================================================================================================
#pragma once

#include <cassert>
#include <cmath>

#include <SFML/Graphics/View.hpp>
#include <SFML/System/Vector2.hpp>

#include "core/ecs/components/scaling_behavior_component.h"
#include "core/ecs/components/sprite_component.h"
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
     * Принципиально: не накапливаем float-ошибки, т.к. каждый resize пересчитывает scale от baseScale.
     * В Debug защищаем инвариант "в компоненты не пишем NaN/Inf" (проверка результата ДО записи).
     */
    class ScalingSystem final : public ISystem {
      public:
        void onResize(World& world, const sf::View& newView) {
            auto view = world.view<ScalingBehaviorComponent, SpriteComponent>();

            // Loop-invariant: размер view одинаков для всех сущностей в рамках одного resize.
            const sf::Vector2f currentViewSize = newView.getSize();

            for (auto [entity, scaling, sprite] : view.each()) {
                (void)entity;

                switch (scaling.kind) {
                case core::ui::ScalingBehaviorKind::Uniform: {
                    const float newUniform =
                        core::ui::computeUniformFactor(currentViewSize, scaling.baseViewSize);

                    // Транзакционное обновление: сначала считаем, затем (в Debug) валидируем, затем пишем.
                    const sf::Vector2f newScale{
                        sprite.baseScale.x * newUniform,
                        sprite.baseScale.y * newUniform
                    };

#if !defined(NDEBUG)
                    const bool scaleFinite = std::isfinite(newScale.x) && std::isfinite(newScale.y);

                    // Engine-level инвариант: NaN/Inf в scale недопустимы.
                    // ВАЖНО: не пишем мусор даже если в IDE нажали "Continue" после assert.
                    assert(scaleFinite &&
                           "ScalingSystem: non-finite scale computed. "
                           "Check computeUniformFactor() and baseScale initialization.");

                    if (!scaleFinite) {
                        break;
                    }
#endif

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