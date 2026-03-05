// ================================================================================================
// File: ecs/systems/player_bounds_system.h
// Purpose: Clamp player position to world bounds (Atrapacielos-specific gameplay rule).
// Used by: Game loop (after MovementSystem).
// ================================================================================================
#pragma once

#include <array>
#include <cmath>

#include <SFML/System/Vector2.hpp>

#include "core/ecs/components/spatial_dirty_tag.h"
#include "core/ecs/components/sprite_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/render/sprite_bounds.h"
#include "core/ecs/system.h"
#include "core/ecs/world.h"

#include "ecs/components/player_tag_component.h"

namespace game::atrapacielos::ecs {

    /**
     * @brief Ограничение игрока в фиксированных логических мировых границах.
     *
     * Правила:
     *  - X: границы спрайта должны оставаться в [0, ширина мира];
     *  - Y (вниз): нижняя граница спрайта не должна быть ниже floorY;
     *  - Нет клампа вверх (бесконечный вертикальный скролл).
     */
    class PlayerBoundsSystem final : public core::ecs::ISystem {
      public:
        PlayerBoundsSystem(const sf::Vector2f worldLogicalSize, const float floorY) noexcept
            : mWorldLogicalSize(worldLogicalSize)
            , mFloorY(floorY) {
        }

        void update(core::ecs::World& world, float) override {
            auto view = world.view<PlayerTagComponent,
                                   core::ecs::TransformComponent,
                                   core::ecs::SpriteComponent>();

            view.each([this, &world](core::ecs::Entity entity,
                                     const PlayerTagComponent&,
                                     core::ecs::TransformComponent& transform,
                                     const core::ecs::SpriteComponent& sprite) {

                const core::spatial::Aabb2 aabb = computeSpriteAabbWithRotation(
                    transform.position,
                    transform.rotationDegrees,
                    sprite.origin,
                    sprite.scale,
                    sprite.textureRect);

                float newX = transform.position.x;
                float newY = transform.position.y;

                if (aabb.minX < 0.f) {
                    newX += -aabb.minX;
                } else if (aabb.maxX > mWorldLogicalSize.x) {
                    newX -= (aabb.maxX - mWorldLogicalSize.x);
                }

                if (aabb.maxY > mFloorY) {
                    newY -= (aabb.maxY - mFloorY);
                }

                if (newX != transform.position.x || newY != transform.position.y) {
                    transform.position = {newX, newY};
                    world.markDirty<core::ecs::SpatialDirtyTag>(entity);
                }
            });
        }

      private:
        [[nodiscard]] static core::spatial::Aabb2 computeSpriteAabbWithRotation(
            const sf::Vector2f position,
            const float rotationDegrees,
            const sf::Vector2f origin,
            const sf::Vector2f scale,
            const sf::IntRect textureRect) noexcept {

            // Fast path: 0 rotation -> existing helper.
            if (rotationDegrees == 0.f) {
                return core::ecs::render::computeSpriteAabbNoRotation(
                    position, origin, scale, textureRect);
            }

            // Размеры в world units с учетом scale.
            const float w = static_cast<float>(textureRect.size.x) * scale.x;
            const float h = static_cast<float>(textureRect.size.y) * scale.y;

            // Origin также масштабируется.
            const float ox = origin.x * scale.x;
            const float oy = origin.y * scale.y;

            // Локальные координаты 4 углов относительно pivot (origin).
            const float left   = -ox;
            const float top    = -oy;
            const float right  = left + w;
            const float bottom = top + h;

            std::array<sf::Vector2f, 4> pts{
                sf::Vector2f{left,  top},
                sf::Vector2f{right, top},
                sf::Vector2f{right, bottom},
                sf::Vector2f{left,  bottom}
            };

            static constexpr float kPi = 3.14159265358979323846f;
            const float rad = rotationDegrees * (kPi / 180.f);
            const float c = std::cos(rad);
            const float s = std::sin(rad);

            float minX = 0.f, maxX = 0.f, minY = 0.f, maxY = 0.f;

            for (std::size_t i = 0; i < pts.size(); ++i) {
                const sf::Vector2f p = pts[i];

                // Стандартная матрица вращения; знак неважен для AABB (берём min/max по 4 углам).
                const float rx = p.x * c - p.y * s;
                const float ry = p.x * s + p.y * c;

                const float wx = position.x + rx;
                const float wy = position.y + ry;

                if (i == 0) {
                    minX = maxX = wx;
                    minY = maxY = wy;
                } else {
                    minX = std::min(minX, wx);
                    maxX = std::max(maxX, wx);
                    minY = std::min(minY, wy);
                    maxY = std::max(maxY, wy);
                }
            }

            core::spatial::Aabb2 aabb{};
            aabb.minX = minX;
            aabb.maxX = maxX;
            aabb.minY = minY;
            aabb.maxY = maxY;
            return aabb;
        }

      private:
        sf::Vector2f mWorldLogicalSize;
        float mFloorY;
    };

} // namespace game::atrapacielos::ecs