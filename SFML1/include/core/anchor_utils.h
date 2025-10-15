#pragma once
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
                case AnchorType::TopCenter:     return { size.x / 2.f, 0.f };
                case AnchorType::TopRight:      return { size.x, 0.f };
                case AnchorType::CenterLeft:    return { 0.f, size.y / 2.f };
                case AnchorType::Center:        return { size.x / 2.f, size.y / 2.f };
                case AnchorType::CenterRight:   return { size.x, size.y / 2.f };
                case AnchorType::BottomLeft:    return { 0.f, size.y };
                case AnchorType::BottomCenter:  return { size.x / 2.f, size.y };
                case AnchorType::BottomRight:   return { size.x, size.y };
                default: return { 0.f, 0.f };
            }
        }

    } // namespace anchors
} // namespace core