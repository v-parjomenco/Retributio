#include "pch.h"

#include "core/ui/ids/ui_id_utils.h"

#include "core/utils/message.h"

namespace core::ui::ids {

    // --------------------------------------------------------------------------------------------
    // enum -> string_view
    // --------------------------------------------------------------------------------------------

    std::string_view toString(AnchorType type) noexcept {
        switch (type) {
        case AnchorType::TopLeft:
            return "TopLeft";
        case AnchorType::TopCenter:
            return "TopCenter";
        case AnchorType::TopRight:
            return "TopRight";
        case AnchorType::CenterLeft:
            return "CenterLeft";
        case AnchorType::Center:
            return "Center";
        case AnchorType::CenterRight:
            return "CenterRight";
        case AnchorType::BottomLeft:
            return "BottomLeft";
        case AnchorType::BottomCenter:
            return "BottomCenter";
        case AnchorType::BottomRight:
            return "BottomRight";
        case AnchorType::None:
        default:
            return "None";
        }
    }

    std::string_view toString(ScalingBehaviorKind kind) noexcept {
        switch (kind) {
        case ScalingBehaviorKind::Uniform:
            return "Uniform";
        case ScalingBehaviorKind::None:
        default:
            return "None";
        }
    }

    std::string_view toString(LockBehaviorKind kind) noexcept {
        switch (kind) {
        case LockBehaviorKind::Screen:
            return "ScreenLock";
        case LockBehaviorKind::World:
        default:
            return "WorldLock";
        }
    }

    // --------------------------------------------------------------------------------------------
    // string_view -> enum
    // --------------------------------------------------------------------------------------------

    [[nodiscard]] AnchorType anchorFromString(std::string_view name,
                                              AnchorType defaultType) noexcept {

        if (name == "TopLeft") {
            return AnchorType::TopLeft;
        }
        if (name == "TopCenter") {
            return AnchorType::TopCenter;
        }
        if (name == "TopRight") {
            return AnchorType::TopRight;
        }
        if (name == "CenterLeft") {
            return AnchorType::CenterLeft;
        }
        if (name == "Center") {
            return AnchorType::Center;
        }
        if (name == "CenterRight") {
            return AnchorType::CenterRight;
        }
        if (name == "BottomLeft") {
            return AnchorType::BottomLeft;
        }
        if (name == "BottomCenter") {
            return AnchorType::BottomCenter;
        }
        if (name == "BottomRight") {
            return AnchorType::BottomRight;
        }
        if (name == "None") {
            return AnchorType::None;
        }

        // Неизвестное значение — лог + откат на defaultType.
        core::utils::message::showError(std::string("[Anchor]\nНеизвестное значение anchor: ") +
                                        std::string{name} + ". Применено значение по умолчанию (" +
                                        std::string{toString(defaultType)} + ").");

        return defaultType;
    }

    [[nodiscard]] ScalingBehaviorKind scalingFromString(std::string_view name,
                                                        ScalingBehaviorKind defaultKind) noexcept {

        if (name == "Uniform") {
            return ScalingBehaviorKind::Uniform;
        }
        if (name == "None") {
            return ScalingBehaviorKind::None;
        }

        core::utils::message::showError(
            std::string("[ScalingBehavior]\nНеизвестное значение resize_scaling: ") +
            std::string{name} + ". Применено значение по умолчанию (" +
            std::string{toString(defaultKind)} + ").");

        return defaultKind;
    }

    [[nodiscard]] LockBehaviorKind lockFromString(std::string_view name,
                                                  LockBehaviorKind defaultKind) noexcept {

        if (name == "ScreenLock") {
            return LockBehaviorKind::Screen;
        }
        if (name == "WorldLock") {
            return LockBehaviorKind::World;
        }

        core::utils::message::showError(
            std::string("[LockBehavior]\nНеизвестное значение lock_behavior: ") +
            std::string{name} + ". Применено значение по умолчанию (" +
            std::string{toString(defaultKind)} + ").");

        return defaultKind;
    }

} // namespace core::ui::ids