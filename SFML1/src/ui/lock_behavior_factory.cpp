#include <unordered_map>
#include <functional>
#include "core/ui/lock_behavior_factory.h"
#include "core/utils/message.h"

namespace core {

    using Creator = std::function<std::unique_ptr<ui::ILockPolicy>()>;

    std::unique_ptr<ui::ILockPolicy> ui::LockBehaviorFactory::create(const std::string& name) {
        static const std::unordered_map<std::string, Creator> factory = {
            { "ScreenLock", []() { return std::make_unique<ScreenLockPolicy>(); } },
            { "WorldLock",  []() { return std::unique_ptr<ILockPolicy>{}; } }
        };

        if (auto it = factory.find(name); it != factory.end()) {
            return it->second();
        }

#ifdef _DEBUG
        assert(false && "[LockBehaviorFactory]\nНеизвестное имя политики, config_loader должен был отфильтровать.");
#endif

        return {};
    }

} // namespace core