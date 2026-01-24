// ================================================================================================
// File: game/skyguard/presentation/background_math.h
// Purpose: Header-only background tiling math utilities for SkyGuard.
// ================================================================================================
#pragma once

#include <cassert>
#include <cmath>

namespace game::skyguard::presentation {

    [[nodiscard]] inline float computeTileStartY(float visibleTop,
                                                 float offset,
                                                 float tileHeight) noexcept {
        assert(tileHeight > 0.0f && "computeTileStartY: tileHeight must be > 0");
        const float normalized = (visibleTop - offset) / tileHeight;
        return std::floor(normalized) * tileHeight + offset;
    }

    [[nodiscard]] inline int computeTileCount(float visibleHeight,
                                              float tileHeight) noexcept {
        assert(tileHeight > 0.0f && "computeTileCount: tileHeight must be > 0");
        assert(visibleHeight >= 0.0f &&
               "computeTileCount: visibleHeight must be >= 0");
        return static_cast<int>(std::ceil(visibleHeight / tileHeight));
    }

} // namespace game::skyguard::presentation