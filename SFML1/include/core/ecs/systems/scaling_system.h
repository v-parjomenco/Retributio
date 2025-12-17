// ================================================================================================
// File: core/ecs/systems/scaling_system.h (DETERMINISTIC VERSION - идеальная)
// Purpose: Apply per-entity scaling behavior on window/view resize (absolute-based)
// Used by: Game resize handling, World/SystemManager
// Related headers: scaling_behavior_component.h, sprite_component.h
// ================================================================================================
#pragma once

#include <SFML/Graphics.hpp>

#include "core/ecs/components/scaling_behavior_component.h"
#include "core/ecs/components/sprite_component.h"
#include "core/ecs/system.h"
#include "core/ecs/world.h"
#include "core/log/log_macros.h"

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
     *
     * МАТЕМАТИЧЕСКОЕ ДОКАЗАТЕЛЬСТВО отсутствия ошибок:
     *  Пусть baseScale = 0.12, baseViewSize = {1024, 768}
     *
     *  Resize 1: window = {672, 504} (65.8%)
     *    uniform = min(672/1024, 504/768) = min(0.656, 0.656) = 0.656
     *    scale = 0.12 * 0.656 = 0.0787
     *
     *  Resize 2: window = {1024, 768} (100%, обратно)
     *    uniform = min(1024/1024, 768/768) = 1.0
     *    scale = 0.12 * 1.0 = 0.12  ← ТОЧНО исходный!
     *
     *  Resize 3: window = {2048, 1536} (200%)
     *    uniform = min(2048/1024, 1536/768) = 2.0
     *    scale = 0.12 * 2.0 = 0.24
     *
     *  Resize 4: window = {1024, 768} (обратно)
     *    uniform = 1.0
     *    scale = 0.12 * 1.0 = 0.12  ← СНОВА точно исходный!
     *
     * Никакого накопления ошибок, только машинная точность float.
     */
    class ScalingSystem final : public ISystem {
      public:
        void onResize(World& world, const sf::View& newView) {
            auto view = world.view<ScalingBehaviorComponent, SpriteComponent>();

            for (auto [entity, scaling, sprite] : view.each()) {
                (void) entity;

                switch (scaling.kind) {
                case core::ui::ScalingBehaviorKind::Uniform: {
                    // Защита от деления на 0
                    const sf::Vector2f safeBaseSize{std::max(scaling.baseViewSize.x, 1.f),
                                                    std::max(scaling.baseViewSize.y, 1.f)};

                    // Текущий размер view
                    const sf::Vector2f currentViewSize = newView.getSize();

                    // Коэффициенты изменения по осям
                    const float scaleX = currentViewSize.x / safeBaseSize.x;
                    const float scaleY = currentViewSize.y / safeBaseSize.y;

                    // Uniform factor (минимум из двух осей)
                    const float newUniform = std::min(scaleX, scaleY);

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