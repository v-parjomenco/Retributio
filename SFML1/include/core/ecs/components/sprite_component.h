// ================================================================================================
// File: core/ecs/components/sprite_component.h
// Purpose: ID-based sprite component for production ECS (hot path, 40 bytes)
// Used by: RenderSystem, SpatialIndexSystem (hot path) (and optional ScalingSystem)
// Related headers: transform_component.h, resource_key.h
// ================================================================================================
#pragma once
#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include "core/resources/keys/resource_key.h"

namespace core::ecs {
    /**
     * @brief Sprite component (data-only, hot path).
     *
     * Контракт:
     *  - texture: RuntimeKey32 текстуры (резолвится через ResourceManager).
     *  - textureRect: ДОЛЖЕН быть явно задан (width/height > 0).
     *  - scale: итоговый масштаб, который использует RenderSystem и SpatialIndexSystem.
     *  - origin: pivot/origin для рендера, вычисляется на границе создания сущности
     *    (например, центр текстуры; anchor/layout — опционально, зависит от игры).
     *  - zOrder: ключ сортировки (детерминизм + batching).
     *
     * Инварианты:
     *  - Только POD-данные (без sf::Sprite / sf::Texture и без owning-ресурсов).
     *  - Компонент должен оставаться компактным (и попадать в 1 cache line).
     * 
     * Примечание:
     *  - Если используется ScalingSystem, "baseScale" хранится в отдельном
     *    SpriteScalingDataComponent и scale пересчитывается при resize.
     *  - Если ScalingSystem не используется (как в SkyGuard), 
     *    scale остаётся authoring/runtime значением; resize обрабатывается через View/viewport.
     */
    struct SpriteComponent {
        /// RuntimeKey32 текстуры (резолвится через ResourceManager)
        core::resources::TextureKey texture{};

        /// Область текстуры для отрисовки (должна быть явно задана, с положительными размерами)
        sf::IntRect textureRect{};

        /// Текущий масштаб (MUTABLE; при наличии ScalingSystem может пересчитываться на resize)
        sf::Vector2f scale{1.f, 1.f};

        /// Pivot/origin для рендера.
        /// Вычисляется один раз при инициализации (обычно центр/низ/и т.п. по контракту игры).
        sf::Vector2f origin{0.f, 0.f};

        /// Z-порядок отрисовки (меньше = дальше/фон, больше = ближе/передний план)
        float zOrder{0.f};
    };

    static_assert(sizeof(SpriteComponent) == 40, "SpriteComponent size changed (update docs).");
    static_assert(std::is_trivially_copyable_v<SpriteComponent>,
                  "SpriteComponent must be trivially copyable.");

} // namespace core::ecs