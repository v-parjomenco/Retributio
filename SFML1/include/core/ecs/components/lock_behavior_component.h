#pragma once

#include <memory>
#include "core/lock_policy.h"

namespace core::ecs {

    /**
    * @brief Компонент, отвечающий за выбор политики фиксации элемента на экране.
    */
    struct LockBehaviorComponent {
        std::unique_ptr<ILockPolicy> policy;
        explicit LockBehaviorComponent(std::unique_ptr<ILockPolicy> p)
            : policy(std::move(p)) {}
    };

} // namespace core::ecs