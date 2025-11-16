// =====================================================
// File: src/core/ecs/systems/movement_system.cpp
// Purpose: position += velocity * dt (perf-aware iteration)
// Used by: MovementSystem
// Related headers: movement_system.h
// =====================================================
#include "pch.h"
#include "core/ecs/systems/movement_system.h"

namespace core::ecs {

    void MovementSystem::update(World& world, float dt) {

        // Берём хранилища

        auto& transforms = world.storage<TransformComponent>();
        auto& velocities = world.storage<VelocityComponent>();

        const bool iterateTransforms = (transforms.size() <= velocities.size());

        // Выбираем меньшее хранилище для итерации, чтобы снизить число map - lookup
        if (iterateTransforms) { // NEW CODE
            for (auto& [entity, transform] : transforms) {
                if (auto* vel = velocities.get(entity)) {
                    transform.position += vel->linear * dt; // простое интегрирование методом Эйлера
                }
            }
        } else { // NEW CODE
            for (auto& [entity, velocity] : velocities) {
                if (auto* tr = transforms.get(entity)) {
                    tr->position += velocity.linear * dt;
                }
            }
        }
    }

} // namespace core::ecs