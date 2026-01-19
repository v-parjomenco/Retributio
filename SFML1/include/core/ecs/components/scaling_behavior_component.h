// ================================================================================================
// File: core/ecs/components/scaling_behavior_component.h
// Purpose: Per-entity scaling behavior settings for resize handling
// Used by: ScalingSystem
// Related headers: core/ui/scaling_behavior.h
// Notes:
//  - NOT used in SkyGuard (no ScalingSystem in SkyGuard), but provided for engine completeness.
// ================================================================================================
#pragma once

#include <SFML/System/Vector2.hpp>

#include "core/ui/scaling_behavior.h"

namespace core::ecs {

    /**
     * @brief Компонент, описывающий поведение масштабирования сущности при resize.
     *
     * Поля:
     *  - kind        — тип масштабирования (Uniform / None);
     *  - baseViewSize — reference view size, под который верстался объект;
     *  - lastUniform — последний рассчитанный коэффициент uniform-скейла.
     *
     * Логика масштабирования реализована в core::ui::computeUniformFactor() и
     * вызывается из ScalingSystem. Здесь только данные.
     */
    struct ScalingBehaviorComponent {
        core::ui::ScalingBehaviorKind kind{core::ui::ScalingBehaviorKind::None};

        // Reference view size, под который верстался объект (обычно стартовое окно игры).
        sf::Vector2f baseViewSize{0.f, 0.f};

        // Последний применённый uniform-множитель.
        float lastUniform{1.f};
    };

} // namespace core::ecs