// ================================================================================================
// File: core/ui/scaling_behavior.h
// Purpose: Scaling behavior types and helper functions for resize handling
// Used by: AnchorProperties, ScalingBehaviorComponent, ScalingSystem
// Related headers: core/ui/ids/ui_id_utils.h
// ================================================================================================
#pragma once

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

        constexpr float kMinViewComponent = 1e-3f;

        // Если baseViewSize неизвестен/некорректен (включая NaN),
        // корректный коэффициент вычислить нельзя -> возвращаем нейтральный множитель.
        if (!(baseViewSize.x > kMinViewComponent) || !(baseViewSize.y > kMinViewComponent)) {
            return 1.0f;
        }

        // Если текущий размер некорректен (включая NaN/0) -> тоже нейтральный множитель.
        if (!(currentViewSize.x > kMinViewComponent) || !(currentViewSize.y > kMinViewComponent)) {
            return 1.0f;
        }

        const float scaleX = currentViewSize.x / baseViewSize.x;
        const float scaleY = currentViewSize.y / baseViewSize.y;

        return std::min(scaleX, scaleY);
    }

} // namespace core::ui