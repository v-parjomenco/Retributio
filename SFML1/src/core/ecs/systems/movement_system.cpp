#include "pch.h"
#include "core/ecs/systems/movement_system.h"

#include "core/ecs/world.h"

namespace core::ecs {

    void MovementSystem::update(World& world, float dt) {
        if (dt == 0.f) {
            return;
        }

        // EnTT view: итерируем только сущности с Transform + Velocity.
        // Внутри view.each() оба компонента гарантированно присутствуют.
        auto view = world.view<TransformComponent, VelocityComponent>();

        view.each([dt](TransformComponent& transform, const VelocityComponent& velocity) {
            transform.position += velocity.linear * dt;
            transform.rotationDegrees += velocity.angularDegreesPerSec * dt;
        });
    }

} // namespace core::ecs