// ================================================================================================
// File: core/ui/lock_behavior.h
// Purpose: Lock behavior types and helpers for resize handling (presentation-level)
// Used by: Optional UI/layout bricks (AnchorProperties), LockBehaviorComponent, LockSystem
// Related headers: core/ui/ids/ui_id_utils.h
// Notes:
//  - Not used by SkyGuard. Kept for potential usage in future games/tools.
// ================================================================================================
#pragma once

#include <SFML/System/Vector2.hpp>

namespace core::ui {

    /**
     * @brief Тип политики фиксации позиции при изменении размера окна.
     */
    enum class LockBehaviorKind {
        World, // жить в мировых координатах
        Screen // фиксировать относительное положение на экране (HUD)
    };

    /**
     * @brief Вычисление новой позиции для ScreenLock (data-driven).
     *
     * Контракт:
     *  - newViewSize уже проверен вызывающим кодом (LockSystem) и валиден (> 0.001).
     *  - previousViewSize может быть неинициализирован/некорректен — тогда позицию не меняем.
     *
     * Политика:
     *  - если previousViewSize некорректен (включая NaN/0) → возвращаем currentPosition.
     */
    [[nodiscard]] inline sf::Vector2f
    computeScreenLockPosition(const sf::Vector2f& currentPosition,
                              const sf::Vector2f& previousViewSize,
                              const sf::Vector2f& newViewSize) noexcept {

        constexpr float kMinViewComponent = 1e-3f;

        // previousViewSize — состояние компонента; может быть неинициализировано.
        // Используем !(val > min), чтобы захватить и малые значения, и NaN.
        if (!(previousViewSize.x > kMinViewComponent) || !(previousViewSize.y > kMinViewComponent)) {
            return currentPosition;
        }

        const sf::Vector2f relativePosition{
            currentPosition.x / previousViewSize.x,
            currentPosition.y / previousViewSize.y
        };

        return {
            newViewSize.x * relativePosition.x,
            newViewSize.y * relativePosition.y
        };
    }

} // namespace core::ui