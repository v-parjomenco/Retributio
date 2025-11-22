// ================================================================================================
// File: core/ecs/components/scaling_behavior_component.h
// Purpose: Per-entity scaling behavior settings for resize handling
// Used by: ScalingSystem
// Related headers: core/ui/scaling_behavior.h
// ================================================================================================
#pragma once

// #include <cstdint>

#include "core/ui/scaling_behavior.h"

namespace core::ecs {

    /**
     * @brief Компонент, описывающий поведение масштабирования сущности при resize.
     *
     * kind        — тип масштабирования (Uniform / None);
     * lastUniform — последний рассчитанный коэффициент uniform-скейла.
     *
     * Логика масштабирования реализована в core::ui::applyUniformScaling и
     * вызывается из ScalingSystem. Здесь только данные.
     */
    struct ScalingBehaviorComponent {
        core::ui::ScalingBehaviorKind kind{core::ui::ScalingBehaviorKind::None};
        float lastUniform{1.f};
    };

} // namespace core::ecs