// ================================================================================================
// File: game/skyguard/config/view_config.h
// Purpose: View configuration for SkyGuard (world/UI logical sizes, camera offsets).
// Used by: ViewManager, view config loader, Game bootstrap.
// Related headers: game/skyguard/presentation/view_manager.h
// ================================================================================================
#pragma once

#include <SFML/System/Vector2.hpp>

namespace game::skyguard::config {

    /**
     * @brief View configuration for SkyGuard.
     *
     * Source of truth: skyguard_game.json
     * Fallback: compile-time defaults below
     *
     * CRITICAL: These values define the gameplay area.
     * Changing them affects game balance, level design, enemy patterns.
     */
    struct ViewConfig {
        // ----------------------------------------------------------------------------------------
        // WORLD VIEW (fixed logical size — NEVER changes at runtime)
        // ----------------------------------------------------------------------------------------

        /// Logical world size in world units.
        /// This is the canonical gameplay area visible to player.
        /// All level design, spawn positions, boundaries use these units.
        sf::Vector2f worldLogicalSize{1920.f, 1080.f};

        // ----------------------------------------------------------------------------------------
        // CAMERA
        // ----------------------------------------------------------------------------------------

        /// Camera offset from player position (world units).
        /// Negative Y = camera looks ahead of player (player at bottom).
        sf::Vector2f cameraOffset{0.f, -100.f};

        /// Minimum camera Y to avoid showing invalid world content.
        /// This is a PRESENTATION clamp, not part of simulation.
        float cameraMinY{540.f};

        // ----------------------------------------------------------------------------------------
        // UI VIEW (fixed logical size — uniform scaling)
        // ----------------------------------------------------------------------------------------

        /// Logical UI size in UI units.
        /// UI scales uniformly with window size using fixed logical UI size.
        sf::Vector2f uiLogicalSize{1920.f, 1080.f};
    };

} // namespace game::skyguard::config