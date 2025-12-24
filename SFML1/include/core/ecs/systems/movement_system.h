// ================================================================================================
// File: core/ecs/systems/movement_system.h
// Purpose: position += velocity * dt
// Used by: Game update loop via World/SystemManager
// Related headers: movement_system.cpp, transform_component.h, velocity_component.h
// ================================================================================================
#pragma once

#include "core/ecs/components/transform_component.h"
#include "core/ecs/components/velocity_component.h"
#include "core/ecs/system.h"

namespace core::ecs {

    /** 
    * @brief Базовая система движения: position += velocity * dt.
    * 
    * Контракт:
    * - работает только по сущностям, у которых есть TransformComponent + VelocityComponent;
    * - выполняет интегрирование Эйлером: position += velocity * dt.
    * 
    * Примечание:
    * - dt должен быть строго положительным; источник dt — TimeService/игровой цикл.
    */
    class MovementSystem final : public ISystem {
      public:
        MovementSystem() = default;
        ~MovementSystem() override = default;

        void update(World& world, float dt) override;
    };

} // namespace core::ecs