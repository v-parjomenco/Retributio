#pragma once

#include <memory>
#include "core/ui/lock_policy.h"

namespace core::ecs {

    /**
    * @brief Компонент, отвечающий за выбор политики фиксации элемента на экране.
    */
    struct LockBehaviorComponent {
        std::unique_ptr<ui::ILockPolicy> policy;
        explicit LockBehaviorComponent(std::unique_ptr<ui::ILockPolicy> p)
            : policy(std::move(p)) {}
    };

} // namespace core::ecs