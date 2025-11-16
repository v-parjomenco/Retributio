#include "pch.h"

#include "core/ui/lock_behavior_factory.h"
#include <cassert>

#include "core/utils/message.h"

namespace core::ui {

    using Creator = std::function<std::unique_ptr<ILockPolicy>()>;

    std::unique_ptr<ILockPolicy> LockBehaviorFactory::create(const std::string& name) {
        static const std::unordered_map<std::string, Creator> factory = {
            {"ScreenLock", []() { return std::make_unique<ScreenLockPolicy>(); }},
            {"WorldLock", []() { return std::unique_ptr<ILockPolicy>{}; }}};

        if (auto it = factory.find(name); it != factory.end()) {
            return it->second();
        }

#ifdef _DEBUG
        assert(false && "[LockBehaviorFactory]\nНеизвестное имя политики, config_loader должен был "
                        "отфильтровать.");
#endif

        return {};
    }

} // namespace core::ui