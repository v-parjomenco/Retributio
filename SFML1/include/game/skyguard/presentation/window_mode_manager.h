// ================================================================================================
// File: game/skyguard/presentation/window_mode_manager.h
// Purpose: Create/recreate SFML RenderWindow in Windowed/Borderless/Fullscreen modes + Alt+Enter.
// Used by: game::skyguard::Game (bootstrap and event handling).
// Related headers: game/skyguard/config/window_config.h
// Notes:
//  - Windowed mode is clamped to the OS work area (taskbar-safe on Windows).
//  - IMPORTANT (Windows): clamp must account for non-client area (titlebar/borders).
//  - Borderless fullscreen: Style::None + State::Windowed at desktop resolution.
//  - Fullscreen: State::Fullscreen at desktop video mode.
// ================================================================================================
#pragma once

#include <string>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Vector2.hpp>

#include "game/skyguard/config/window_config.h"

namespace game::skyguard::presentation {

    class WindowModeManager {
      public:
        void init(const config::WindowConfig& cfg);

        [[nodiscard]] bool createInitial(sf::RenderWindow& window) noexcept;

        void requestToggleWindowedBorderless() noexcept { mToggleRequested = true; }

        [[nodiscard]] bool applyPending(sf::RenderWindow& window) noexcept;

        void onWindowResized(const sf::Vector2u& newSize) noexcept;

        [[nodiscard]] config::WindowMode getMode() const noexcept { return mMode; }

      private:
        struct WorkArea {
            int left = 0;
            int top = 0;
            int width = 0;
            int height = 0;
        };

        struct WindowedState {
            sf::Vector2u size{};          // client size (runtime last-known)
            sf::Vector2i position{0, 0};  // window top-left
        };

        [[nodiscard]] bool applyMode(sf::RenderWindow& window, config::WindowMode newMode) noexcept;

        [[nodiscard]] static WorkArea getWorkAreaPrimary() noexcept;

        [[nodiscard]] static sf::Vector2u clampWindowedClientToWorkArea(sf::Vector2u desiredClient,
                                                                        const WorkArea& workArea) noexcept;

        [[nodiscard]] static sf::Vector2i clampWindowedPosition(sf::Vector2i desiredPos,
                                                                sf::Vector2u clientSize,
                                                                const WorkArea& workArea) noexcept;

      private:
        config::WindowMode mMode{config::WindowMode::Windowed};
        std::string mTitle;

        sf::Vector2u mDesiredWindowedSize{1920u, 1080u};

        WindowedState mSavedWindowed{};
        bool mHasSavedWindowed = false;

        bool mToggleRequested = false;
    };

} // namespace game::skyguard::presentation