#include "pch.h"
#include "core/ecs/systems/movement_system.h"

#include <cassert>
#include "core/ecs/world.h"

namespace core::ecs {

    void MovementSystem::update(World& world, float dt) {
        assert((dt > 0.f) && "MovementSystem expects positive dt");
        // Runtime-guard для Release: отсекает и dt <= 0, и NaN.
        if (!(dt > 0.f)) {
            return;
        }

        // EnTT view: итерируем только сущности с Transform + Velocity.
        // Внутри view.each() оба компонента гарантированно присутствуют.
        auto view = world.view<TransformComponent, VelocityComponent>();

        view.each([dt](TransformComponent& transform, const VelocityComponent& velocity) {
            transform.position += velocity.linear * dt;
        });
    }

} // namespace core::ecs