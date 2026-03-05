// ================================================================================================
// File: core/config/properties/anchor_properties.h
// Purpose: Data-only anchor/resize/lock configuration brick (optional UI/layout)
// Used by: Optional UI/HUD blueprints, tools/editors, future games
// Related headers: core/ui/ids/ui_id_utils.h
// Notes:
//  - Not used by Atrapacielos. Kept for potential usage in future games/tools.
// ================================================================================================
#pragma once

#include <SFML/System/Vector2.hpp>

#include "core/ui/anchor_utils.h"
#include "core/ui/lock_behavior.h"
#include "core/ui/scaling_behavior.h"

namespace core::config::properties {

    /**
     * @brief Набор параметров, связанных с позиционированием и поведением при resize.
     *
     * Эта структура описывает:
     *  - к какому якорю привязан объект (anchorType),
     *  - где он находится по умолчанию, если якорь не задан,
     *  - как он масштабируется при изменении размера окна,
     *  - какую политику фиксации (lock) использовать.
     *
     * Здесь хранятся только данные, без логики. Логику реализуют AnchorPolicy,
     * ScalingSystem и LockSystem в других модулях.
     */
    struct AnchorProperties {

        /**
         * @brief Тип якоря (обычно парсится лоадером из строкового поля вроде "anchor").
         *
         * AnchorType::None означает "нет якоря" (объект живёт в startPosition).
         *
         * Строковое имя в рантайме не храним — при необходимости его можно
         * всегда восстановить через core::ui::ids::toString(anchorType).
         */
        core::ui::AnchorType anchorType{core::ui::AnchorType::None};

        /**
        * @brief Стартовая позиция в мировых координатах, используемая,
        *        если anchorType == AnchorType::None (JSON поле "start_position").
        */
        sf::Vector2f startPosition{0.f, 0.f};

        /**
         * @brief Тип политики масштабирования при изменении размера экрана.
         *
         * Обычно парсится лоадером из строкового поля (например "resize_scaling")
         * с помощью core::ui::ids::scalingFromString(...).
         *
         * ScalingBehaviorKind::None    — без дополнительного масштабирования;
         * ScalingBehaviorKind::Uniform — равномерное масштабирование.
         */
        core::ui::ScalingBehaviorKind scalingBehavior{core::ui::ScalingBehaviorKind::None};

        /**
         * @brief Тип политики фиксации при resize.
         *
         * Обычно парсится лоадером из строкового поля (например "lock_behavior")
         * с помощью core::ui::ids::lockFromString(...).
         *
         * LockBehaviorKind::World  — жить в мировых координатах;
         * LockBehaviorKind::Screen — фиксировать относительное положение на экране.
         */
        core::ui::LockBehaviorKind lockBehavior{core::ui::LockBehaviorKind::World};
    };

} // namespace core::config::properties