// ================================================================================================
// File: core/ecs/components/scaling_behavior_component.h
// Purpose: Per-entity scaling behavior settings for resize handling
// Used by: ScalingSystem, PlayerInitSystem
// Related headers: core/ui/scaling_behavior.h, game\skyguard\ecs\systems\player_init_system.h
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
     * Логика масштабирования реализована в core::ui::applyUniformScaling и
     * вызывается из ScalingSystem. Здесь только данные.
     */
    struct ScalingBehaviorComponent {
        core::ui::ScalingBehaviorKind kind{core::ui::ScalingBehaviorKind::None};

        // Reference view size, под который верстался объект (обычно стартовое окно игры).
        sf::Vector2f baseViewSize{0.f, 0.f};

        // Последний применённый uniform-множитель (см. applyUniformScaling).
        float lastUniform{1.f};
    };

} // namespace core::ecs