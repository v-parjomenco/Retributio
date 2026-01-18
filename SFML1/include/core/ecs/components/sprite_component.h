// ================================================================================================
// File: core/ecs/components/sprite_component.h
// Purpose: ID-based sprite component for production ECS (hot path, 40 bytes)
// Used by: RenderSystem, SpatialIndexSystem, ScalingSystem (all hot paths)
// Related headers: transform_component.h, resource_key.h, sprite_scaling_data_component.h
// ================================================================================================
#pragma once
#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include "core/resources/keys/resource_key.h"

namespace core::ecs {
    /**
     * @brief Production ID-based sprite component (hot path, cache-optimized).
     *
     * АРХИТЕКТУРНОЕ РЕШЕНИЕ:
     *  - Компонент хранит ТОЛЬКО данные (no sf::Sprite, no methods)
     *  - RenderSystem разрешает TextureKey → sf::Texture через ResourceManager
     *  - Размер: 40 байт (умещается в 1 cache line = 64 байт)
     *
     * ПОЛЯ (детерминированная модель):
     *  - texture:     RuntimeKey32 текстуры (4 байта)
     *  - textureRect: область текстуры для sprite sheets (16 байт)
     *  - scale:       текущий масштаб = baseScale * uniformFactor (8 байт, MUTABLE)
     *  - origin:      точка привязки для anchor'ов (8 байт)
     *  - zOrder:      слой отрисовки (4 байта)
     *
     * HOT/COLD DATA SEPARATION:
     *  - baseScale (холодные данные) → отдельный компонент SpriteScalingDataComponent
     *
     * КРИТИЧНО: scale vs baseScale (детерминизм):
     *  - baseScale — конфиг из JSON (хранится в SpriteScalingDataComponent)
     *  - scale — текущий масштаб, пересчитывается ScalingSystem
     *  - Формула: scale = baseScale * uniformFactor (no накопление float ошибок)
     *
     * ORIGIN (для anchor'ов):
     *  - Вычисляется один раз при инициализации (PlayerInitSystem)
     *  - Определяет точку привязки спрайта (center, bottom-center, etc.)
     *  - RenderSystem использует origin при генерации вершин
     *
     * ПРЕИМУЩЕСТВА:
     *  - Компактность: 40 байт
     *  - Data-driven: текстура по ID, легко менять/стримить
     *  - Детерминизм: scale пересчитывается от baseScale
     *  - Z-ordering: готовность к слоистому рендерингу
     */
    struct SpriteComponent {
        /// RuntimeKey32 текстуры (резолвится через ResourceManager)
        core::resources::TextureKey texture{};

        /// Область текстуры для отрисовки (должна быть явно задана, с положительными размерами)
        sf::IntRect textureRect{};

        /// Текущий масштаб (MUTABLE, обновляется ScalingSystem при resize)
        sf::Vector2f scale{1.f, 1.f};

        /// Точка привязки (origin) для anchor'ов
        /// Вычисляется один раз при инициализации из anchor type
        sf::Vector2f origin{0.f, 0.f};

        /// Z-порядок отрисовки (меньше = дальше/фон, больше = ближе/передний план)
        float zOrder{0.f};
    };

} // namespace core::ecs