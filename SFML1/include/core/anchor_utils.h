#pragma once

#include <string>
#include <SFML/System/Vector2.hpp>
#include "core/config.h"

namespace core {

    enum class AnchorType {
        None,
        TopLeft, TopCenter, TopRight,
        CenterLeft, Center, CenterRight,
        BottomLeft, BottomCenter, BottomRight
    };

    namespace anchors {

        inline sf::Vector2f computeAnchorOffset(const sf::Vector2f& size, AnchorType type) {
            switch (type) {
                case AnchorType::TopLeft:       return { 0.f, 0.f };
                case AnchorType::TopCenter:     return { size.x * 0.5f, 0.f };
                case AnchorType::TopRight:      return { size.x, 0.f };
                case AnchorType::CenterLeft:    return { 0.f, size.y * 0.5f };
                case AnchorType::Center:        return { size.x * 0.5f, size.y * 0.5f };
                case AnchorType::CenterRight:   return { size.x, size.y * 0.5f };
                case AnchorType::BottomLeft:    return { 0.f, size.y };
                case AnchorType::BottomCenter:  return { size.x * 0.5f, size.y };
                case AnchorType::BottomRight:   return { size.x, size.y };
                default:                        return { 0.f, 0.f };
            }
        }

        inline AnchorType fromString(const std::string& str) noexcept {
            if (str == "TopLeft")       return AnchorType::TopLeft;
            if (str == "TopCenter")     return AnchorType::TopCenter;
            if (str == "TopRight")      return AnchorType::TopRight;
            if (str == "CenterLeft")    return AnchorType::CenterLeft;
            if (str == "Center")        return AnchorType::Center;
            if (str == "CenterRight")   return AnchorType::CenterRight;
            if (str == "BottomLeft")    return AnchorType::BottomLeft;
            if (str == "BottomCenter")  return AnchorType::BottomCenter;
            if (str == "BottomRight")   return AnchorType::BottomRight;
            if (str == "None")          return AnchorType::None;
            return AnchorType::None; // значение по умолчанию при ошибке
        }

    } // namespace anchors
} // namespace core