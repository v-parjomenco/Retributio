#pragma once

#include "core/ui/scaling_policy.h"

namespace core::ecs {

    /**
     * @brief Компонент масштабирования на ресайз
     * Mode::Uniform — применяем равномерное масштабирование (через UniformScalingPolicy)
     * Mode::None    — масштабирование отключено
     */
    struct ScalingBehaviorComponent {
        enum class Mode { None, Uniform };
        Mode mode{ Mode::None };

        // Политика хранит per-entity состояние (mLastUniform), что правильно для ECS.
        core::ui::UniformScalingPolicy policy{};
    };

} // namespace core::ecs