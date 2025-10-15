#include "core/resize_behavior_factory.h"

namespace core {

std::unique_ptr<IResizeBehavior> ResizeBehaviorFactory::create(const std::string& name) {
    if (name == "FixedWorldScaling")
        return std::make_unique<FixedWorldScalingBehavior>();
    else if (name == "FixedWorldNoScaling")
        return std::make_unique<FixedWorldNoScalingBehavior>();
    else if (name == "FixedScreenScaling")
        return std::make_unique<FixedScreenScalingBehavior>();
    else if (name == "FixedScreenNoScaling")
        return std::make_unique<FixedScreenNoScalingBehavior>();

    // Значение по умолчанию
    return std::make_unique<FixedWorldNoScalingBehavior>();
}

} // namespace core