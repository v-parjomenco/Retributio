// ================================================================================================
// File: core/ui/scaling_behavior.h
// Purpose: Scaling behavior types and helper functions for resize handling
// Used by: AnchorProperties, ScalingBehaviorComponent, ScalingSystem
// Related headers: core/ui/ids/ui_id_utils.h
// ================================================================================================
#pragma once

#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/View.hpp>
#include <SFML/System/Vector2.hpp>

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
     *  - baseViewSize задаётся при создании сущности (reference size),
     *    обычно равен начальному размеру окна игры;
     *  - lastUniform — последний применённый равномерный коэффициент масштабирования.
     *
     * Алгоритм:
     *  - newUniform = min(currentView.x / baseViewSize.x,
     *                     currentView.y / baseViewSize.y);
     *  - ratio = newUniform / lastUniform;
     *  - sprite.scale *= ratio;
     *  - lastUniform = newUniform.
     *
     * Таким образом масштаб спрайта всегда равен baseScale * newUniform.
     */
    void applyUniformScaling(sf::Sprite& sprite, const sf::View& view,
                             const sf::Vector2f& baseViewSize, float& lastUniform);

} // namespace core::ui