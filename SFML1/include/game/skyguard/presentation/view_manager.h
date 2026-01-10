// ================================================================================================
// File: game/skyguard/presentation/view_manager.h
// Purpose: Manage WorldView and UiView for SkyGuard (letterbox + UI separation).
// Used by: game::skyguard::Game.
// Related headers: game/skyguard/config/view_config.h, core/ui/viewport_utils.h
// ================================================================================================
#pragma once

#include <SFML/Graphics/View.hpp>
#include <SFML/System/Vector2.hpp>

#include "game/skyguard/config/view_config.h"

namespace game::skyguard::presentation {

    /**
     * @brief Manages WorldView and UiView for SkyGuard.
     *
     * Responsibilities:
     *  - Initialize views from config
     *  - Handle resize (update viewport, not size)
     *  - Provide views for render passes
     *
     * NOT an ECS system. Owned by Game.
     */
    class ViewManager {
      public:
        /**
         * @brief Initialize views from configuration.
         *
         * @param config View configuration
         * @param initialWindowSize Window size at startup
         */
        void init(const config::ViewConfig& config,
                  const sf::Vector2u& initialWindowSize) noexcept;

        /**
         * @brief Handle window resize.
         *
         * Updates viewport (letterbox) for WORLD view only.
         * Neither view size changes — only viewport.
         *
         * @param newWindowSize New window size in pixels
         */
        void onResize(const sf::Vector2u& newWindowSize) noexcept;

        /**
         * @brief Update camera center (call after player movement).
         *
         * @param targetPosition Player position in world space (bottom-center).
         */
        void updateCamera(const sf::Vector2f& targetPosition) noexcept;

        [[nodiscard]] const sf::View& getWorldView() const noexcept { return mWorldView; }
        [[nodiscard]] const sf::View& getUiView() const noexcept { return mUiView; }
        [[nodiscard]] sf::Vector2f getWorldLogicalSize() const noexcept { return mWorldLogicalSize; }
        [[nodiscard]] sf::Vector2f getUiLogicalSize() const noexcept { return mUiLogicalSize; }
        [[nodiscard]] sf::Vector2f getCameraOffset() const noexcept { return mCameraOffset; }
        [[nodiscard]] float getCameraMinY() const noexcept { return mCameraMinY; }

      private:
        sf::View mWorldView;
        sf::View mUiView;

        sf::Vector2f mWorldLogicalSize{1920.f, 1080.f};
        sf::Vector2f mUiLogicalSize{1920.f, 1080.f};
        sf::Vector2f mCameraOffset{0.f, -100.f};
        float mCameraMinY{540.f};

        sf::Vector2u mCurrentWindowSize{1920u, 1080u};
    };

} // namespace game::skyguard::presentation