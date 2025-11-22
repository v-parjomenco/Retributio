// ================================================================================================
// File: core/ui/scaling_behavior.h
// Purpose: Scaling behavior types and helper functions for resize handling
// Used by: AnchorProperties, ScalingBehaviorComponent, ScalingSystem
// Related headers: core/config.h, core/ui/ids/ui_id_utils.h
// ================================================================================================
#pragma once

#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/View.hpp>

namespace core::ui {

    /**
     * @brief Тип политики масштабирования при изменении размера окна.
     *
     * None    - без дополнительного масштабирования;
     * Uniform - равномерное масштабирование относительно базового размера окна.
     *
     * Строковые имена ("None", "Uniform") и маппинг string <-> enum живут
     * в core::ui::ids::scalingFromString / core::ui::ids::toString.
     */
    enum class ScalingBehaviorKind { None, Uniform };

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
    void applyUniformScaling(sf::Sprite& sprite, const sf::View& view, float& lastUniform);

} // namespace core::ui