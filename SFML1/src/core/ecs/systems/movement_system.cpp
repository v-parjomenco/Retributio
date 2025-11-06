#include "core/ecs/systems/movement_system.h"

namespace core::ecs {

    void MovementSystem::update(World& world, float dt) {

        // Берём хранилища

        auto& transforms = world.storage<TransformComponent>();
        auto& velocities = world.storage<VelocityComponent>();

        // Итерируемся по ВСЕМ трансформам,
        // хранящимся в ComponentStorage<TransformComponent> (внутри это unordered_map<Entity, TransformComponent>)
        // (можно оптимизировать: итерироваться по меньшему из двух storage)
        for (auto& [entity, transform] : transforms) {
            if (auto* vel = velocities.get(entity)) { // есть ли скорость у этой же сущности? 
                transform.position += vel->linear * dt;
            }
        }
    }

} // namespace core::ecs