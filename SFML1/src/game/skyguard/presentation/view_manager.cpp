#include "pch.h"

#include "game/skyguard/presentation/view_manager.h"

#include <algorithm>

#include "core/log/log_macros.h"
#include "core/ui/viewport_utils.h"

namespace game::skyguard::presentation {

    void ViewManager::init(const config::ViewConfig& config,
                           const sf::Vector2u& initialWindowSize) noexcept {
        if (!(config.worldLogicalSize.x > 0.f) || !(config.worldLogicalSize.y > 0.f)) {
            LOG_WARN(core::log::cat::Config,
                     "ViewManager: invalid worldLogicalSize (x={}, y={}), "
                     "using fallback 1920x1080",
                     config.worldLogicalSize.x,
                     config.worldLogicalSize.y);
            mWorldLogicalSize = {1920.f, 1080.f};
        } else {
            mWorldLogicalSize = config.worldLogicalSize;
        }

        if (!(config.uiLogicalSize.x > 0.f) || !(config.uiLogicalSize.y > 0.f)) {
            LOG_WARN(core::log::cat::Config,
                     "ViewManager: invalid uiLogicalSize (x={}, y={}), "
                     "using fallback 1920x1080",
                     config.uiLogicalSize.x,
                     config.uiLogicalSize.y);
            mUiLogicalSize = {1920.f, 1080.f};
        } else {
            mUiLogicalSize = config.uiLogicalSize;
        }

        mCameraOffset = config.cameraOffset;

        if (!(config.cameraMinY >= 0.f)) {
            LOG_WARN(core::log::cat::Config,
                     "ViewManager: invalid cameraMinY ({}), using fallback {}",
                     config.cameraMinY,
                     mWorldLogicalSize.y * 0.5f);
            mCameraMinY = mWorldLogicalSize.y * 0.5f;
        } else {
            mCameraMinY = config.cameraMinY;
        }

        mCurrentWindowSize = initialWindowSize;

        // WORLD VIEW: fixed logical size, letterbox viewport
        mWorldView.setSize(mWorldLogicalSize);
        mWorldView.setCenter(mWorldLogicalSize * 0.5f);
        mWorldView.setViewport(core::ui::computeLetterboxViewport(
            initialWindowSize, mWorldLogicalSize));

        // UI VIEW: fixed logical size, full window viewport
        mUiView.setSize(mUiLogicalSize);
        mUiView.setCenter(mUiLogicalSize * 0.5f);
        mUiView.setViewport(sf::FloatRect({0.f, 0.f}, {1.f, 1.f}));
    }

    void ViewManager::onResize(const sf::Vector2u& newWindowSize) noexcept {
        if (newWindowSize.x == 0u || newWindowSize.y == 0u) {
            return;
        }

        mCurrentWindowSize = newWindowSize;

        const sf::FloatRect worldViewport = core::ui::computeLetterboxViewport(
            newWindowSize, mWorldLogicalSize);
        mWorldView.setViewport(worldViewport);
    }

    void ViewManager::updateCamera(const sf::Vector2f& targetPosition) noexcept {
        const float desiredCenterY = targetPosition.y + mCameraOffset.y;
        const float baseCenterY = mCameraMinY;
        const float centerY = (desiredCenterY < baseCenterY) ? desiredCenterY : baseCenterY;

        mWorldView.setCenter({mWorldLogicalSize.x * 0.5f, centerY});
    }

} // namespace game::skyguard::presentation