// ================================================================================================
// File: game/skyguard/ecs/systems/player_bounds_system.h
// Purpose: Clamp player position to world bounds (SkyGuard-specific gameplay rule).
// Used by: Game loop (after MovementSystem).
// ================================================================================================
#pragma once

#include <SFML/System/Vector2.hpp>

#include "core/ecs/components/spatial_dirty_tag.h"
#include "core/ecs/components/sprite_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/render/sprite_bounds.h"
#include "core/ecs/system.h"
#include "core/ecs/world.h"

#include "game/skyguard/ecs/components/player_tag_component.h"

namespace game::skyguard::ecs {

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
                const core::spatial::Aabb2 aabb = core::ecs::render::computeSpriteAabbNoRotation(
                    transform.position,
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
        sf::Vector2f mWorldLogicalSize;
        float mFloorY;
    };

} // namespace game::skyguard::ecs