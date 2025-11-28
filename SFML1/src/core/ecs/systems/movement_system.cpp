#include "pch.h"

#include "core/ecs/systems/movement_system.h"

namespace core::ecs {

    void MovementSystem::update(World& world, float dt) {

        // Берём хранилища

        auto& transforms = world.storage<TransformComponent>();
        auto& velocities = world.storage<VelocityComponent>();

        // Чтобы снизить количество map-lookups (unordered_map::find) в горячем цикле,
        // итерируемся по меньшему из двух хранилищ:
        //
        //  - если Transform'ов меньше или столько же, сколько Velocity:
        //      * идём по TransformStorage
        //      * для каждой сущности пытаемся найти её Velocity
        //
        //  - иначе:
        //      * идём по VelocityStorage
        //      * для каждой сущности пытаемся найти её Transform
        //
        // Таким образом, количество хеш-поисков в среднем = min(|T|, |V|),
        // а не max(|T|, |V|), что важно для больших миров.
        const bool iterateTransforms = (transforms.size() <= velocities.size());

        if (iterateTransforms) {
            for (auto& [entity, transform] : transforms) {
                if (auto* vel = velocities.get(entity)) {
                    // Простое интегрирование методом Эйлера:
                    // position += velocity * dt
                    transform.position += vel->linear * dt;
                }
            }
        } else {
            for (auto& [entity, velocity] : velocities) {
                if (auto* tr = transforms.get(entity)) {
                    tr->position += velocity.linear * dt;
                }
            }
        }
    }

} // namespace core::ecs