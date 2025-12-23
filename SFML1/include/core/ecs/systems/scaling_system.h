// ================================================================================================
// File: core/ecs/systems/scaling_system.h (DETERMINISTIC VERSION)
// Purpose: Apply per-entity scaling behavior on window/view resize (absolute-based)
// Used by: Game resize handling, World/SystemManager
// Related headers: scaling_behavior_component.h, sprite_component.h
// ================================================================================================
#pragma once

#include <SFML/Graphics/View.hpp>

#include "core/ecs/components/scaling_behavior_component.h"
#include "core/ecs/components/sprite_component.h"
#include "core/ecs/entity.h"
#include "core/ecs/system.h"
#include "core/ecs/world.h"
#include "core/log/log_macros.h"
#include "core/ui/scaling_behavior.h"

namespace core::ecs {

    /**
     * @brief Scaling system (ДЕТЕРМИНИРОВАННАЯ ВЕРСИЯ - без накопления ошибок).
     *
     * КРИТИЧЕСКИЕ ПРЕИМУЩЕСТВА:
     *  - Нет накопления float ошибок (всегда: scale = baseScale * uniform)
     *  - Детерминизм: одинаковый размер окна → одинаковый scale
     *  - Простота: один оператор присваивания
     *
     * АЛГОРИТМ (Uniform scaling, детерминированный):
     *  1. Вычисляем newUniform = min(newView.x / baseView.x, newView.y / baseView.y)
     *  2. Присваиваем: sprite.scale = sprite.baseScale * newUniform
     *  3. Сохраняем: lastUniform = newUniform (для дебага/метрик)
     *
     * ВАЖНО:
     *  - sprite.baseScale IMMUTABLE (устанавливается один раз в PlayerInitSystem)
     *  - sprite.scale MUTABLE (пересчитывается каждый resize)
     *  - lastUniform нужен ТОЛЬКО для логов/метрик (можно убрать в Release)
     */
    class ScalingSystem final : public ISystem {
      public:
        void onResize(World& world, const sf::View& newView) {
            auto view = world.view<ScalingBehaviorComponent, SpriteComponent>();

            for (auto [entity, scaling, sprite] : view.each()) {
                (void) entity;

                switch (scaling.kind) {
                case core::ui::ScalingBehaviorKind::Uniform: {
                    const float newUniform =
                        core::ui::computeUniformFactor(newView, scaling.baseViewSize);

                    // DEBUG: логируем ДО изменения
                    LOG_TRACE(core::log::cat::ECS,
                              "ScalingSystem: entity={}, BEFORE: scale=({:.3f}, {:.3f}), "
                              "baseScale=({:.3f}, {:.3f}), uniform={:.3f}",
                              core::ecs::toUint(entity), sprite.scale.x, sprite.scale.y,
                              sprite.baseScale.x, sprite.baseScale.y, newUniform);

                    // ДЕТЕРМИНИРОВАННОЕ ПРИСВАИВАНИЕ (всегда от baseScale!)
                    sprite.scale.x = sprite.baseScale.x * newUniform;
                    sprite.scale.y = sprite.baseScale.y * newUniform;

                    // DEBUG: логируем ПОСЛЕ изменения
                    LOG_TRACE(core::log::cat::ECS,
                              "ScalingSystem: entity={}, AFTER: scale=({:.3f}, {:.3f})",
                              core::ecs::toUint(entity), sprite.scale.x, sprite.scale.y);

                    // Сохраняем для дебага/метрик (можно убрать в Release)
                    scaling.lastUniform = newUniform;

                    break;
                }

                case core::ui::ScalingBehaviorKind::None:
                    // Ничего не делаем
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