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
        Uniform // равномерное масштабирование (сохранение пропорций по меньшей стороне)
    };

    /**
     * @brief Вычисление uniform factor по размерам.
     *
     * Контракт:
     *  - baseViewSize валиден и задаётся движком (проверяется на границе записи, не здесь).
     *  - currentViewSize может быть 0/NaN при сворачивании/краевых состояниях окна — это штатно.
     *
     * Политика:
     *  - если currentViewSize некорректен → возвращаем 1.0f (нейтральный множитель).
     *  - без логов/assert'ов: функция leaf-level, без дублей валидации.
     */
    [[nodiscard]] inline float computeUniformFactor(const sf::Vector2f& currentViewSize,
                                                    const sf::Vector2f& baseViewSize) noexcept {
        constexpr float kMinViewComponent = 1e-3f;

        // currentViewSize — данные от OS: 0/NaN возможны (minimized). Не падаем.
        // Используем !(val > min), чтобы захватить и малые значения, и NaN.
        if (!(currentViewSize.x > kMinViewComponent) || !(currentViewSize.y > kMinViewComponent)) {
            return 1.0f;
        }

        const float scaleX = currentViewSize.x / baseViewSize.x;
        const float scaleY = currentViewSize.y / baseViewSize.y;
        return std::min(scaleX, scaleY);
    }

    /**
     * @brief Хелпер для вычисления фактора напрямую из sf::View.
     */
    [[nodiscard]] inline float computeUniformFactor(const sf::View& view,
                                                    const sf::Vector2f& baseViewSize) noexcept {
        return computeUniformFactor(view.getSize(), baseViewSize);
    }

} // namespace core::ui