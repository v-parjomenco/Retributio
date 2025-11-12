// =====================================================
// File: core/ecs/systems/movement_system.h
// Purpose: position += velocity * dt
// Used by: Game update loop via World/SystemManager
// Related headers: movement_system.cpp, transform_component.h, velocity_component.h
// =====================================================
#pragma once

#include "core/ecs/components/transform_component.h"
#include "core/ecs/components/velocity_component.h"
#include "core/ecs/system.h"
#include "core/ecs/world.h"


namespace core::ecs {

    /** 
    * @brief Система движения
    * проходит по всем сущностям, у которых есть и Transform и Velocity,
    * и выполняет: position += velocity * dt
    */
    class MovementSystem : public ISystem {
    public:
        MovementSystem() = default;
        ~MovementSystem() override = default;

        void update(World& world, float dt) override;
    };

} // namespace core::ecs