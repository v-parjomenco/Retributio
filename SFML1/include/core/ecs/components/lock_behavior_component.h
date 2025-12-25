// ================================================================================================
// File: core/ecs/components/lock_behavior_component.h
// Purpose: Per-entity state for lock behavior on window resize
// Used by: LockSystem, PlayerInitSystem
// Related headers: core/ui/lock_behavior.h, game/skyguard/ecs/systems/player_init_system.h
// ================================================================================================
#pragma once

#include <SFML/System/Vector2.hpp>

#include "core/ui/lock_behavior.h"

namespace core::ecs {

    /**
    * @brief Компонент, описывающий поведение фиксации сущности при resize.
    *
    * Данные:
    *  - kind             — какая политика применяется (World / Screen);
    *  - previousViewSize — размер view на предыдущем шаге (для ScreenLock);
    *  - initialized      — был ли уже инициализирован previousViewSize.
    *
    * Логика политики реализована в LockSystem::onResize().
    * Здесь только данные, без какой-либо логики.
    */
    struct LockBehaviorComponent {
        core::ui::LockBehaviorKind kind{core::ui::LockBehaviorKind::World};

        // Размер view, который использовался в прошлый раз для позиционирования.
        sf::Vector2f previousViewSize{0.f, 0.f};
    };

} // namespace core::ecs