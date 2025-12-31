// ================================================================================================
// File: core/ecs/render/sprite_bounds.h
// Purpose: Sprite bounds helpers (AABB computation, no allocations)
// Used by: RenderSystem, SpatialIndexSystem
// ================================================================================================
#pragma once

#include <algorithm>
#include <cmath>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include "core/spatial/aabb2.h"
#include "core/utils/math_constants.h"

namespace core::ecs::render {

    /**
     * @brief Проверка, что textureRect задан явно (положительные размеры).
     */
    [[nodiscard]] inline bool hasExplicitRect(const sf::IntRect& rect) noexcept {
        return (rect.size.x > 0) && (rect.size.y > 0);
    }

    /**
     * @brief AABB для спрайта без поворота (rotationDegrees == 0).
     */
    [[nodiscard]] inline core::spatial::Aabb2 computeSpriteAabbNoRotation(
        const sf::Vector2f& position,
        const sf::Vector2f& origin,
        const sf::Vector2f& scale,
        const sf::IntRect& rect) noexcept
    {
        const float w = static_cast<float>(rect.size.x);
        const float h = static_cast<float>(rect.size.y);

        const float ox = origin.x;
        const float oy = origin.y;
        const float sx = scale.x;
        const float sy = scale.y;
        const float px = position.x;
        const float py = position.y;
        
        // Быстрый путь без поворота:
        // left/top — позиция левого верхнего угла после origin+scale,
        // затем добавляем (w*sx, h*sy). Для отрицательных sx/sy (отзеркаливание)
        // min/max берём через std::min/max.
        const float left = px - (ox * sx);
        const float top  = py - (oy * sy);
        
        const float right  = left + (w * sx);
        const float bottom = top  + (h * sy);

        core::spatial::Aabb2 out{};
        out.minX = std::min(left, right);
        out.maxX = std::max(left, right);
        out.minY = std::min(top, bottom);
        out.maxY = std::max(top, bottom);
        return out;
    }

    /**
     * @brief AABB для спрайта с поворотом, sin/cos должны быть заранее вычислены.
     */
    [[nodiscard]] inline core::spatial::Aabb2 computeSpriteAabbRotated(
        const sf::Vector2f& position,
        const sf::Vector2f& origin,
        const sf::Vector2f& scale,
        const sf::IntRect& rect,
        const float cachedSin,
        const float cachedCos) noexcept
    {
        const float w = static_cast<float>(rect.size.x);
        const float h = static_cast<float>(rect.size.y);

        const float ox = origin.x;
        const float oy = origin.y;
        const float sx = scale.x;
        const float sy = scale.y;

        const sf::Vector2f l0{(-ox)    * sx, (-oy)    * sy};
        const sf::Vector2f l1{(w - ox) * sx, (-oy)    * sy};
        const sf::Vector2f l2{(w - ox) * sx, (h - oy) * sy};
        const sf::Vector2f l3{(-ox)    * sx, (h - oy) * sy};

        const auto rotate = [&](const sf::Vector2f& v) noexcept -> sf::Vector2f {
            return {
                (v.x * cachedCos) - (v.y * cachedSin),
                (v.x * cachedSin) + (v.y * cachedCos)
            };
        };

        const sf::Vector2f p0 = rotate(l0) + position;
        const sf::Vector2f p1 = rotate(l1) + position;
        const sf::Vector2f p2 = rotate(l2) + position;
        const sf::Vector2f p3 = rotate(l3) + position;

        const float minX = std::min(std::min(p0.x, p1.x), std::min(p2.x, p3.x));
        const float maxX = std::max(std::max(p0.x, p1.x), std::max(p2.x, p3.x));
        const float minY = std::min(std::min(p0.y, p1.y), std::min(p2.y, p3.y));
        const float maxY = std::max(std::max(p0.y, p1.y), std::max(p2.y, p3.y));

        core::spatial::Aabb2 out{};
        out.minX = minX;
        out.minY = minY;
        out.maxX = maxX;
        out.maxY = maxY;
        return out;
    }

    /**
     * @brief AABB с быстрым путём для rotationDegrees == 0.
     */
    [[nodiscard]] inline core::spatial::Aabb2 computeSpriteAabb(
        const sf::Vector2f& position,
        const sf::Vector2f& origin,
        const sf::Vector2f& scale,
        const sf::IntRect& rect,
        const float rotationDegrees) noexcept
    {
        if (rotationDegrees == 0.f) {
            return computeSpriteAabbNoRotation(position, origin, scale, rect);
        }

        const float radians = rotationDegrees * core::utils::kDegToRad;
        const float s = std::sin(radians);
        const float c = std::cos(radians);
        return computeSpriteAabbRotated(position, origin, scale, rect, s, c);
    }

} // namespace core::ecs::render