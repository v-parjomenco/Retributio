// ================================================================================================
// File: core/ui/scaling_behavior.h
// Purpose: Scaling behavior types and helper functions for resize handling
// Used by: AnchorProperties, ScalingBehaviorComponent, ScalingSystem
// Related headers: core/ui/ids/ui_id_utils.h
// ================================================================================================
#pragma once

#include <algorithm>
#include <SFML/Graphics/View.hpp>
#include <SFML/System/Vector2.hpp>

namespace core::ui {

    /**
     * @brief Тип политики масштабирования при изменении размера окна.
     */
    enum class ScalingBehaviorKind {
        None,   // без дополнительного масштабирования
        Uniform // равномерное масштабирование
    };

    /**
     * @brief Вычисление uniform factor БЕЗ мутации sf::Sprite (data-driven).
     *
     *
     * Новый подход (детерминированный):
     *   float uniformFactor = computeUniformFactor(view, baseViewSize);
     *   spriteComp.scale = spriteComp.baseScale * uniformFactor;
     */
    [[nodiscard]] inline float computeUniformFactor(const sf::View& view,
                                                    const sf::Vector2f& baseViewSize) noexcept {

        const sf::Vector2f currentViewSize = view.getSize();

        // Не делим на 0, даже если окно пришло в “нулевой” размер.
        const float safeBaseX = std::max(baseViewSize.x, 1.0f);
        const float safeBaseY = std::max(baseViewSize.y, 1.0f);

        const float scaleX = currentViewSize.x / safeBaseX;
        const float scaleY = currentViewSize.y / safeBaseY;

        return std::min(scaleX, scaleY);
    }

} // namespace core::ui