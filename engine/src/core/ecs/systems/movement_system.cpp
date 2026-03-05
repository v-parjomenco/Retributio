#include "pch.h"

#include "core/ecs/systems/movement_system.h"

#include "core/ecs/components/spatial_dirty_tag.h"
#include "core/ecs/validation/numeric_integrity.h"
#include "core/ecs/world.h"

namespace core::ecs {

    void MovementSystem::update(World& world, float dt) {
        if (dt == 0.f) {
            return;
        }

        // EnTT view: итерируем только сущности с Transform + Velocity.
        // Внутри view.each() оба компонента гарантированно присутствуют.
        auto view = world.view<TransformComponent, VelocityComponent>();

        view.each([dt, &world]([[maybe_unused]] Entity entity,
                               TransformComponent& transform,
                               const VelocityComponent& velocity) {
            const bool hasLinear = (velocity.linear.x != 0.f) || (velocity.linear.y != 0.f);
            const bool hasAngular = (velocity.angularDegreesPerSec != 0.f);

            if (!(hasLinear || hasAngular)) {
                return;
            }

            transform.position += velocity.linear * dt;
            transform.rotationDegrees += velocity.angularDegreesPerSec * dt;

            // Validate-on-write: арифметика (velocity * dt) — основной источник NaN/Inf
            // в runtime mutation pipeline. Первичный барьер перед spatial index и render.
            validation::assertTransformFinite(transform, "MovementSystem::update");

            world.markDirty<SpatialDirtyTag>(entity);
        });
    }

} // namespace core::ecs