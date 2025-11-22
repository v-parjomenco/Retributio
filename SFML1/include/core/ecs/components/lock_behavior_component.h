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
    * Логика политики реализована в core::ui::applyScreenLock и вызывается
    * из LockSystem. Здесь только данные.
    */
    struct LockBehaviorComponent {
        core::ui::LockBehaviorKind kind{core::ui::LockBehaviorKind::World};
        sf::Vector2f previousViewSize{};
        bool initialized{false};
    };

} // namespace core::ecs