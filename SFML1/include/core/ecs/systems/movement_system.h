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
     *  - работает только по сущностям, у которых есть TransformComponent + VelocityComponent;
     *  - выполняет интегрирование Эйлером:
     *      position += linear * dt
     *      rotationDegrees += angularDegreesPerSec * dt
     *
     * Примечание:
     *  - dt валидируется на write-boundary (TimeService).
     *  - при dt == 0.f система делает ранний выход (no-op).
     */
    class MovementSystem final : public ISystem {
      public:
        MovementSystem() = default;
        ~MovementSystem() override = default;

        void update(World& world, float dt) override;
    };

} // namespace core::ecs