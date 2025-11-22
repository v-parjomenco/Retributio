// ================================================================================================
// File: core/ui/lock_behavior.h
// Purpose: Lock behavior types and helpers for resize handling
// Used by: AnchorProperties, LockBehaviorComponent, LockSystem
// Related headers: core/config.h, core/ui/ids/ui_id_utils.h
// ================================================================================================
#pragma once

#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/View.hpp>
#include <SFML/System/Vector2.hpp>

namespace core::ui {

    /**
     * @brief Тип политики фиксации позиции при изменении размера окна.
     *
     * World  - жить в мировых координатах, LockSystem не трогает позицию спрайта
     *          (кроме синхронизации Transform).
     * Screen - фиксировать относительное положение на экране (как HUD).
     *
     * Строковые имена ("WorldLock", "ScreenLock") и маппинг string <-> enum
     * живут в core::ui::ids::lockFromString / core::ui::ids::toString.
     */
    enum class LockBehaviorKind { World, Screen };

    /**
     * @brief Логика ScreenLock-политики.
     *
     * Поведение:
     *  - первая инициализация — mPreviousViewSize = (WINDOW_WIDTH, WINDOW_HEIGHT);
     *  - далее позиция считается в долях от предыдущего размера и переносится в новый view.
     *
     * Состояние (previousViewSize/initialized) передаётся снаружи, чтобы
     * храниться в ECS-компоненте, а не во внутреннем объекте.
     */
    void applyScreenLock(sf::Sprite& sprite, const sf::View& view, sf::Vector2f& previousViewSize,
                         bool& initialized);

} // namespace core::ui