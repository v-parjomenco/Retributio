#pragma once

#include <memory>
#include <utility>

#include "core/ui/lock_policy.h"

namespace core::ecs {

    /**
    * @brief Компонент, отвечающий за выбор политики фиксации элемента на экране.
    * Хранит уникальный указатель на политику фиксации (core::ui::ILockPolicy),
    * которая инкапсулирует логику привязки спрайта к экрану/миру.
    */
    struct LockBehaviorComponent {
        std::unique_ptr<core::ui::ILockPolicy> policy;

        explicit LockBehaviorComponent(std::unique_ptr<core::ui::ILockPolicy> p)
            : policy(std::move(p)) {
        }
    };

} // namespace core::ecs