// ================================================================================================
// File: core/ecs/components/sprite_scaling_data_component.h
// Purpose: Cold-path sprite scaling configuration data (resize-only, 8 bytes)
// Used by: ScalingSystem (onResize), PlayerInitSystem (one-shot write)
// Related headers: sprite_component.h, scaling_system.h
// ================================================================================================
#pragma once
#include <SFML/System/Vector2.hpp>

namespace core::ecs {
    /**
     * @brief Конфигурационные данные масштабирования спрайта (cold component).
     *
     * АРХИТЕКТУРНОЕ РЕШЕНИЕ (hot/cold data separation):
     *  - baseScale читается ТОЛЬКО при window resize (редкое событие)
     *  - Отделён от SpriteComponent (hot path: RenderSystem, SpatialIndexSystem @ 60 fps)
     *  - Размер: 8 bytes
     *
     * КОНТРАКТ:
     *  - baseScale — IMMUTABLE конфигурационное значение из JSON
     *  - Задаётся один раз при инициализации (PlayerInitSystem)
     *  - Валидируется в config_loader (> 0), здесь trust-on-read
     *  - ScalingSystem пересчитывает: sprite.scale = baseScale * uniformFactor
     *
     * ПОЧЕМУ ОТДЕЛЬНЫЙ КОМПОНЕНТ:
     *  - Cache efficiency: не грузим baseScale в hot path (RenderSystem iterate)
     *  - Семантика: это конфигурационные данные, не runtime state
     *  - Масштабируемость: при 500k sprites экономим 4 MB cache bandwidth/frame
     *
     * ПРИМЕР ИСПОЛЬЗОВАНИЯ:
     *  // Init (one-shot):
     *  world.addComponent<SpriteScalingDataComponent>(entity, {.baseScale = {0.12f, 0.12f}});
     *
     *  // Resize (rare):
     *  const auto& scalingData = world.getComponent<SpriteScalingDataComponent>(entity);
     *  sprite.scale = scalingData.baseScale * newUniform;
     */
    struct SpriteScalingDataComponent {
        /// Базовый масштаб из конфига (IMMUTABLE, устанавливается при init)
        /// Пример: {0.12f, 0.12f} для player sprite
        /// Валидация: config_loader гарантирует > 0
        sf::Vector2f baseScale{1.f, 1.f};
    };

} // namespace core::ecs