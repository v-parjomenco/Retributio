// ================================================================================================
// File: core/ecs/components/sprite_component.h
// Purpose: ID-based sprite component for production ECS (100K+ entities)
// Used by: RenderSystem, ScalingSystem, LockSystem, game-specific init systems
// Related headers: transform_component.h, resource_ids.h
// ================================================================================================
#pragma once

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include "core/resources/ids/resource_ids.h"

namespace core::ecs {

    /**
     * @brief Production ID-based sprite component (Phase 3 EnTT migration).
     *
     * АРХИТЕКТУРНОЕ РЕШЕНИЕ:
     *  - Компонент хранит ТОЛЬКО данные (no sf::Sprite, no methods)
     *  - RenderSystem разрешает TextureID → sf::Texture через ResourceManager
     *  - Размер: ~44 байта
     *
     * ПОЛЯ (детерминированная модель):
     *  - textureId:   enum ID текстуры (4 байта)
     *  - textureRect: область текстуры для sprite sheets (16 байт)
     *  - baseScale:   базовый масштаб из конфига (8 байт, IMMUTABLE)
     *  - scale:       текущий масштаб = baseScale * uniformFactor (8 байт, mutable)
     *  - origin:      точка привязки для anchor'ов (8 байт)
     *  - zOrder:      слой отрисовки (4 байта)
     *
     * КРИТИЧНО: baseScale vs scale
     *  - baseScale = из конфига (например {0.12, 0.12}), НИКОГДА не меняется
     *  - scale = текущий масштаб после Uniform scaling
     *  - ScalingSystem ВСЕГДА пересчитывает: scale = baseScale * newUniform
     *  - Это гарантирует детерминизм (no float накопление ошибок)
     *
     * ORIGIN (для anchor'ов):
     *  - Вычисляется один раз при инициализации (PlayerInitSystem)
     *  - Определяет "точку привязки" спрайта (center, bottom-center, etc.)
     *  - RenderSystem использует origin при setOrigin(sprite)
     *
     * ПРЕИМУЩЕСТВА:
     *  Компактность
     *  Cache-friendly: плотная упаковка в EnTT storage
     *  Data-driven: текстура по ID, легко менять/стримить
     *  Детерминизм: scale = baseScale * uniform (no накопление ошибок)
     *  Z-ordering: готовность к слоистому рендерингу
     */
    struct SpriteComponent {
        /// ID текстуры из enum (резолвится через ResourceManager)
        core::resources::ids::TextureID textureId{core::resources::ids::TextureID::Unknown};

        /// Область текстуры для отрисовки (должна быть явно задана, с положительными размерами)
        sf::IntRect textureRect{};

        /// Базовый масштаб из конфига (IMMUTABLE, устанавливается при init)
        /// Пример: {0.12f, 0.12f} для player
        sf::Vector2f baseScale{1.f, 1.f};

        /// Текущий масштаб (mutable, обновляется ScalingSystem)
        /// Всегда: scale = baseScale * uniformFactor
        sf::Vector2f scale{1.f, 1.f};

        /// Точка привязки (origin) для anchor'ов
        /// Вычисляется один раз при инициализации из anchor type
        sf::Vector2f origin{0.f, 0.f};

        /// Z-порядок отрисовки (меньше = дальше/фон, больше = ближе/передний план)
        float zOrder{0.f};
    };

} // namespace core::ecs