// ================================================================================================
// File: core/ui/lock_behavior.h
// Purpose: Lock behavior types and helpers for resize handling
// Used by: AnchorProperties, LockBehaviorComponent, LockSystem
// Related headers: core/ui/ids/ui_id_utils.h
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
     * @brief Вычисление новой позиции для ScreenLock БЕЗ мутации sf::Sprite (data-driven).
     *
     *   sf::Vector2f newPos = computeScreenLockPosition(currentPos, prevViewSize, newViewSize);
     *   transformComp.position = newPos;
     */
    [[nodiscard]] inline sf::Vector2f
    computeScreenLockPosition(const sf::Vector2f& currentPosition,
                              const sf::Vector2f& previousViewSize,
                              const sf::Vector2f& newViewSize) noexcept {

        constexpr float kMinViewComponent = 1e-3f;

        // Если previousViewSize неизвестен/некорректен (включая NaN),
        // относительное положение не восстановить.
        // В этом случае не двигаем позицию, чтобы избежать "телепорта" на первом resize.
        if (!(previousViewSize.x > kMinViewComponent) ||
            !(previousViewSize.y > kMinViewComponent)) {
            return currentPosition;
        }

        // Если новый размер некорректен (включая NaN/0), тоже ничего не делаем.
        if (!(newViewSize.x > kMinViewComponent) || !(newViewSize.y > kMinViewComponent)) {
            return currentPosition;
        }

        const sf::Vector2f relativePosition = {currentPosition.x / previousViewSize.x,
                                               currentPosition.y / previousViewSize.y};

        return {newViewSize.x * relativePosition.x, newViewSize.y * relativePosition.y};
    }

} // namespace core::ui