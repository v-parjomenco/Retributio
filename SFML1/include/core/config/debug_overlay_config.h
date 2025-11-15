#pragma once

#include <string>

#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>

namespace core::config {

    struct DebugOverlayConfig {
        bool enabled = true;
        sf::Vector2f position{10.f, 10.f};
        unsigned int characterSize = 35;
        sf::Color color{255, 0, 0, 255};
    };

    DebugOverlayConfig loadDebugOverlayConfig(const std::string& path);

} // namespace core::config