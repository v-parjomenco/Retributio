#include "pch.h"

#include "game/skyguard/presentation/view_manager.h"

#include <algorithm>
#include <cassert>

#include "core/ui/viewport_utils.h"

namespace game::skyguard::presentation {

    void ViewManager::init(const config::ViewConfig& config,
                           const sf::Vector2u& initialWindowSize) noexcept {
#if !defined(NDEBUG)
        assert(config.worldLogicalSize.x > 0.f && config.worldLogicalSize.y > 0.f &&
               "ViewManager::init: worldLogicalSize must be > 0 (validated in loader)");
        assert(config.uiLogicalSize.x > 0.f && config.uiLogicalSize.y > 0.f &&
               "ViewManager::init: uiLogicalSize must be > 0 (validated in loader)");
        assert(config.cameraCenterYMax >= 0.f &&
               "ViewManager::init: cameraCenterYMax must be >= 0 (validated in loader)");
        assert(initialWindowSize.x > 0u && initialWindowSize.y > 0u &&
               "ViewManager::init: initial window size must be > 0");
#endif
        mWorldLogicalSize = config.worldLogicalSize;
        mUiLogicalSize = config.uiLogicalSize;
        mCameraOffset = config.cameraOffset;
        mCameraCenterYMax = config.cameraCenterYMax;

        mCurrentWindowSize = initialWindowSize;

        // WORLD VIEW: фиксированный логический размер, letterbox viewport
        mWorldView.setSize(mWorldLogicalSize);
        mWorldView.setCenter(mWorldLogicalSize * 0.5f);
        mWorldView.setViewport(core::ui::computeLetterboxViewport(
            initialWindowSize, mWorldLogicalSize));

        // UI VIEW: фиксированный логический размер, viewport на всё окно
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

        // SFML world space: +Y вниз.
        // Камера не должна "откатываться вниз" ниже стартовой точки, поэтому:
        // centerY ограничиваем максимумом по числу Y (max center):
        // centerY = min(desiredCenterY, cameraCenterYMax).
        const float centerY = std::min(desiredCenterY, mCameraCenterYMax);

        // X намеренно зафиксирован по центру мира для вертикального скроллера.
        mWorldView.setCenter({mWorldLogicalSize.x * 0.5f, centerY});
    }

} // namespace game::skyguard::presentation