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
#include "core/ui/anchor_utils.h"
#include "core/ui/lock_behavior.h"
#include "core/ui/scaling_behavior.h"

namespace core::config::properties {

    /**
     * @brief Набор параметров, связанных с позиционированием и поведением при resize.
     *
     * Эта структура описывает:
     *  - как объект привязан к экрану (anchorName),
     *  - где он находится по умолчанию, если якорь не задан,
     *  - как он масштабируется при изменении размера окна,
     *  - какую политику фиксации (lock) использовать.
     *
     * Здесь хранятся только данные, без логики. Логику реализуют AnchorPolicy,
     * ResizeBehavior и LockPolicy в других модулях.
     */
    struct AnchorProperties {
        /**
        * @brief Строковое представление якоря из JSON (поле "anchor").
        *        Значение "None" означает, что якорь не используется.
        */
        std::string anchorName = ::config::DEFAULT_ANCHOR;

        /**
         * @brief Тип якоря, сконвертированный из строки anchorName в loader'е.
         *
         * AnchorType::None означает "нет якоря" (объект живёт в startPosition).
         * Это поле используется в runtime-коде (PlayerInitSystem, UI и т.п.),
         * чтобы не дёргать fromString(...) повторно.
         */
        core::ui::AnchorType anchorType{core::ui::AnchorType::None};

        /**
        * @brief Стартовая позиция в мировых координатах, используемая,
        *        если anchorName == "None" (JSON поле "anchor").
        */
        sf::Vector2f startPosition{0.f, 0.f};

        /**
         * @brief Тип политики масштабирования при изменении размера экрана.
         *
         * В JSON это строка "resize_scaling", которая конвертируется в enum
         * (см. core::ui::scalingBehaviorFromString).
         *
         * ScalingBehaviorKind::None    — без дополнительного масштабирования;
         * ScalingBehaviorKind::Uniform — равномерное масштабирование.
         */
        core::ui::ScalingBehaviorKind scalingBehavior{core::ui::ScalingBehaviorKind::None};

        /**
         * @brief Тип политики фиксации при resize.
         *
         * В JSON это строка "lock_behavior", которая конвертируется в enum
         * (см. core::ui::lockBehaviorFromString).
         *
         * LockBehaviorKind::World  — жить в мировых координатах;
         * LockBehaviorKind::Screen — фиксировать относительное положение на экране.
         */
        core::ui::LockBehaviorKind lockBehavior{core::ui::LockBehaviorKind::World};
    };

} // namespace core::config::properties