// ================================================================================================
// File: core/config/properties/anchor_properties.h
// Purpose: Data-only anchor/resize/lock configuration brick
// Used by: PlayerBlueprint, future UI/world-anchored blueprints
// Related headers: core/config.h
// ================================================================================================
#pragma once

#include <string>

#include <SFML/System/Vector2.hpp>

#include "core/config.h"

namespace core::config::properties {

/**
 * @brief Набор параметров, связанных с позиционированием и поведением при resize.
 *
 * Эта структура описывает:
 *  - как объект привязан к экрану (anchor),
 *  - где он находится по умолчанию, если якорь не задан,
 *  - как он масштабируется при изменении размера окна,
 *  - какую политику фиксации (lock) использовать.
 *
 * Здесь хранятся только данные, без логики. Логику реализуют AnchorPolicy,
 * ResizeBehavior и LockPolicy в других модулях.
 */
struct AnchorProperties {
    /// @brief Строковое представление якоря (например, "BottomCenter", "None").
    std::string anchorName = ::config::DEFAULT_ANCHOR;

    /// @brief Стартовая позиция в мировых координатах, если anchor == "None".
    sf::Vector2f startPosition{0.f, 0.f};

    /// @brief Политика масштабирования при изменении размера экрана.
    ///
    /// Сейчас используются значения:
    ///  - "Uniform" — равномерное масштабирование,
    ///  - "None"    — без масштабирования.
    std::string scaling = ::config::DEFAULT_SCALING;

    /// @brief Политика фиксации (lock_behavior), передаётся в LockBehaviorFactory.
    ///
    /// Примеры значений:
    ///  - "ScreenLock"
    ///  - "WorldLock"
    std::string lockBehavior = ::config::DEFAULT_LOCK_BEHAVIOR;
};

} // namespace core::config::properties