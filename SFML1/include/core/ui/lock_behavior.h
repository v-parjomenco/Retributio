// ================================================================================================
// File: core/ui/lock_behavior.h
// Purpose: Lock behavior types and helpers for resize handling
// Used by: AnchorProperties, LockBehaviorComponent, LockSystem
// Related headers: core/ui/ids/ui_id_utils.h
// ================================================================================================
#pragma once

#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/View.hpp>
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
    inline sf::Vector2f computeScreenLockPosition(const sf::Vector2f& currentPosition,
                                                  const sf::Vector2f& previousViewSize,
                                                  const sf::Vector2f& newViewSize) {
        const float previousWidth = std::max(1.f, previousViewSize.x);
        const float previousHeight = std::max(1.f, previousViewSize.y);

        const sf::Vector2f relativePosition = {currentPosition.x / previousWidth,
                                               currentPosition.y / previousHeight};

        return {newViewSize.x * relativePosition.x, newViewSize.y * relativePosition.y};
    }

} // namespace core::ui