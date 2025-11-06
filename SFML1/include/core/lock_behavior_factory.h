#pragma once

#include <memory>
#include <string>
#include "core/lock_policy.h"

namespace core {

    /**
     * @brief Фабрика политик фиксации (lock_behavior)
     *
     * Поддерживаемые значения (регистр важен):
     * - "ScreenLock" -> ScreenLockPolicy (фиксация относительно экрана)
     * - "WorldLock"  -> nullptr          (никакой фиксации; жить в мировых координатах)
     */
    class LockBehaviorFactory {
    public:
        static std::unique_ptr<ILockPolicy> create(const std::string& name);
    };

} // namespace core