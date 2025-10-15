#pragma once
#include "core/resize_behavior.h"
#include <memory>
#include <string>

namespace core {

    class ResizeBehaviorFactory {
    public:
        static std::unique_ptr<IResizeBehavior> create(const std::string& name);
    };

} // namespace core