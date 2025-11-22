// ================================================================================================
// File: core/ui/lock_behavior.h
// Purpose: Lock behavior types and helpers for resize handling
// Used by: AnchorProperties, LockBehaviorComponent, LockSystem, config loaders
// Related headers: core/config.h
// ================================================================================================
#pragma once

#include <string_view>

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
 */
enum class LockBehaviorKind {
    World,
    Screen
};

/**
 * @brief Конвертация строкового имени политики (из JSON) в enum.
 *
 * Поддерживаемые значения:
 *  - "ScreenLock" -> LockBehaviorKind::Screen
 *  - "WorldLock"  -> LockBehaviorKind::World
 *
 * При неизвестном имени:
 *  - возвращается defaultKind,
 *  - при этом можно залогировать/показать ошибку на уровне конфиг-лоадера.
 */
LockBehaviorKind lockBehaviorFromString(
    std::string_view name,
    LockBehaviorKind defaultKind = LockBehaviorKind::World);

/**
 * @brief Логика ScreenLock-политики.
 *
 * Поведение полностью повторяет прежний ScreenLockPolicy:
 *  - первая инициализация — mPreviousViewSize = (WINDOW_WIDTH, WINDOW_HEIGHT);
 *  - далее позиция считается в долях от предыдущего размера и переносится в новый view.
 *
 * Состояние (previousViewSize/initialized) передаётся снаружи, чтобы
 * храниться в ECS-компоненте, а не во внутреннем объекте.
 */
void applyScreenLock(sf::Sprite& sprite,
                     const sf::View& view,
                     sf::Vector2f& previousViewSize,
                     bool& initialized);

} // namespace core::ui