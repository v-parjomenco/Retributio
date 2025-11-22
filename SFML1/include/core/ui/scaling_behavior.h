// ================================================================================================
// File: core/ui/scaling_behavior.h
// Purpose: Scaling behavior types and helper functions for resize handling
// Used by: AnchorProperties, ScalingBehaviorComponent, ScalingSystem, config loaders
// Related headers: core/config.h
// ================================================================================================
#pragma once

#include <string_view>

#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/View.hpp>

namespace core::ui {

    /**
     * @brief Тип политики масштабирования при изменении размера окна.
     *
     * None    - без дополнительного масштабирования;
     * Uniform - равномерное масштабирование относительно базового размера окна.
     */
    enum class ScalingBehaviorKind {
        None,
        Uniform
    };

    /**
     * @brief Конвертация строкового имени resize_scaling (из JSON) в enum.
     *
     * Поддерживаемые значения:
     *  - "Uniform" -> ScalingBehaviorKind::Uniform
     *  - "None"    -> ScalingBehaviorKind::None
     *
     * При неизвестном имени:
     *  - возвращается defaultKind,
     *  - при этом лог и/или popup об ошибке.
     */
    ScalingBehaviorKind scalingBehaviorFromString(
        std::string_view name,
        ScalingBehaviorKind defaultKind = ScalingBehaviorKind::None);

    /**
     * @brief Реализация Uniform-скейлинга.
     *
     *  - baseViewSize берётся из config::WINDOW_WIDTH/HEIGHT;
     *  - newUniform = min(scaleX, scaleY);
     *  - ratio = newUniform / lastUniform;
     *  - sprite.scale *= ratio;
     *  - lastUniform обновляется.
     *
     * Вся "память" хранится в lastUniform (per-entity state в компоненте).
     */
    void applyUniformScaling(sf::Sprite& sprite,
                             const sf::View& view,
                             float& lastUniform);

} // namespace core::ui