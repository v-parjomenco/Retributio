// ================================================================================================
// File: presentation/window_mode_manager.h
// Purpose: Create/recreate SFML RenderWindow in Windowed/Borderless/Fullscreen modes + Alt+Enter.
// Used by: game::atrapacielos::Game (bootstrap and event handling).
// Related headers: config/window_config.h
// Notes:
//  - Windowed mode is clamped to the OS work area (taskbar-safe on Windows).
//  - IMPORTANT (Windows): clamp must account for non-client area (titlebar/borders).
//  - Borderless fullscreen: Style::None + State::Windowed at desktop resolution.
//  - Fullscreen: State::Fullscreen at desktop video mode.
//  - Alt+Enter behavior: cycle Windowed -> Borderless -> Fullscreen -> Windowed.
// ================================================================================================
#pragma once

#include <string>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Vector2.hpp>

#include "config/window_config.h"

namespace game::atrapacielos::presentation {

    class WindowModeManager {
      public:
        void init(const config::WindowConfig& cfg, std::string windowTitle);

        [[nodiscard]] bool createInitial(sf::RenderWindow& window) noexcept;

        // Запросить циклическое переключение режимов (Alt+Enter).
        void requestCycleMode() noexcept {
            mCycleRequested = true;
        }

        // Применить отложенное переключение режимов одним действием (вне pollEvent цикла).
        [[nodiscard]] bool applyPending(sf::RenderWindow& window) noexcept;

        // Для Windowed: обновляем last-known client size (после resize).
        void onWindowResized(const sf::Vector2u& newSize) noexcept;

        [[nodiscard]] config::WindowMode getMode() const noexcept {
            return mMode;
        }

      private:
        struct WorkArea {
            int left = 0;
            int top = 0;
            int width = 0;
            int height = 0;
        };

        struct WindowedState {
            sf::Vector2u size{};         // client size (runtime last-known)
            sf::Vector2i position{0, 0}; // window top-left
        };

        [[nodiscard]] bool applyMode(sf::RenderWindow& window, config::WindowMode newMode) noexcept;

        [[nodiscard]] static WorkArea getWorkAreaPrimary() noexcept;

        [[nodiscard]] static sf::Vector2u
        clampWindowedClientToWorkArea(sf::Vector2u desiredClient,
                                      const WorkArea& workArea) noexcept;

        [[nodiscard]] static sf::Vector2i clampWindowedPosition(sf::Vector2i desiredPos,
                                                                sf::Vector2u clientSize,
                                                                const WorkArea& workArea) noexcept;

      private:
        config::WindowMode mMode{config::WindowMode::Windowed};

        // Заголовок окна — app-level значение (app.display_name). Храним копию для create().
        std::string mTitle;

        sf::Vector2u mDesiredWindowedSize{1920u, 1080u};

        WindowedState mSavedWindowed{};
        bool mHasSavedWindowed = false;

        bool mCycleRequested = false;
    };

} // namespace game::atrapacielos::presentation
