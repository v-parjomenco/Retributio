#include "pch.h"

#include "game/skyguard/presentation/window_mode_manager.h"
#include <algorithm>
#include <cstdint>
#include <utility>

#include <SFML/Window/ContextSettings.hpp>
#include <SFML/Window/VideoMode.hpp>

#include "core/log/log_macros.h"

#if defined(_WIN32)
#include "core/compiler/platform/windows.h"

#include <Windows.h>
#endif

namespace game::skyguard::presentation {

    void WindowModeManager::init(const config::WindowConfig& cfg, std::string windowTitle) {
        mMode = cfg.mode;
        mTitle = std::move(windowTitle);
        mDesiredWindowedSize = {cfg.width, cfg.height};

        // Стартовое "восстановление" windowed режима — желаемые значения из конфига.
        mSavedWindowed.size = mDesiredWindowedSize;
        mSavedWindowed.position = {0, 0};
        mHasSavedWindowed = false;

        // validate-on-write: title не должен быть пустым (но мы всё равно держим fallback).
        if (mTitle.empty()) {
            mTitle = "SkyGuard";
        }
    }

    bool WindowModeManager::createInitial(sf::RenderWindow& window) noexcept {
        return applyMode(window, mMode);
    }

    bool WindowModeManager::applyPending(sf::RenderWindow& window) noexcept {
        if (!mCycleRequested) {
            return false;
        }
        mCycleRequested = false;

        // Alt+Enter: цикл 3 режимов.
        config::WindowMode target = config::WindowMode::Windowed;
        switch (mMode) {
        case config::WindowMode::Windowed:
            target = config::WindowMode::BorderlessFullscreen;
            break;
        case config::WindowMode::BorderlessFullscreen:
            target = config::WindowMode::Fullscreen;
            break;
        case config::WindowMode::Fullscreen:
            target = config::WindowMode::Windowed;
            break;
        }

        return applyMode(window, target);
    }

    void WindowModeManager::onWindowResized(const sf::Vector2u& newSize) noexcept {
        if (mMode != config::WindowMode::Windowed) {
            return;
        }

        mSavedWindowed.size = newSize;
        mHasSavedWindowed = true;
    }

    bool WindowModeManager::applyMode(sf::RenderWindow& window,
                                      config::WindowMode newMode) noexcept {
        if (mTitle.empty()) {
            mTitle = "SkyGuard";
        }

        // ВАЖНО: не запрашиваем Core Profile/атрибуты контекста — 
        // SFML Graphics требует compatibility.
        // Здесь просим только sRGB-capable framebuffer.
        const sf::ContextSettings srgbContextSettings{
            .sRgbCapable = true
        };

        const auto logSrgbAvailability = [&window]() {
            const auto& used = window.getSettings();
            if (!used.sRgbCapable) {
                LOG_WARN(core::log::cat::Engine,
                         "sRGB framebuffer not available; sRGB textures may look incorrect");
            }
        };

        // При уходе из Windowed сохраняем размер/позицию для восстановления.
        if (mMode == config::WindowMode::Windowed && newMode != config::WindowMode::Windowed) {
            mSavedWindowed.size = window.getSize();
            mSavedWindowed.position = window.getPosition();
            mHasSavedWindowed = true;
        }

        const sf::VideoMode desktop = sf::VideoMode::getDesktopMode();

        if (newMode == config::WindowMode::BorderlessFullscreen) {
            const sf::VideoMode mode({desktop.size.x, desktop.size.y}, desktop.bitsPerPixel);

            window.create(mode,
                          mTitle,
                          sf::Style::None,
                          sf::State::Windowed,
                          srgbContextSettings);
            window.setPosition({0, 0});

            if (!window.isOpen()) {
                LOG_ERROR(core::log::cat::Engine,
                          "Failed to create BorderlessFullscreen window ({}x{}).",
                          mode.size.x,
                          mode.size.y);
                return false;
            }

            logSrgbAvailability();
            mMode = newMode;
            return true;
        }

        if (newMode == config::WindowMode::Fullscreen) {
            const sf::VideoMode mode({desktop.size.x, desktop.size.y}, desktop.bitsPerPixel);

            window.create(mode, 
                          mTitle, 
                          sf::Style::Default, 
                          sf::State::Fullscreen,
                          srgbContextSettings);

            if (!window.isOpen()) {
                LOG_ERROR(core::log::cat::Engine,
                          "Failed to create Fullscreen window ({}x{}).",
                          mode.size.x,
                          mode.size.y);
                return false;
            }

            logSrgbAvailability();
            mMode = newMode;
            return true;
        }

        // Windowed
        {
            const WorkArea workArea = getWorkAreaPrimary();

            sf::Vector2u desiredClient = mDesiredWindowedSize;
            sf::Vector2i desiredPos{workArea.left, workArea.top};

            if (mHasSavedWindowed) {
                desiredClient = mSavedWindowed.size;
                desiredPos = mSavedWindowed.position;
            }

            // ВАЖНО: clamp учитывает non-client (рамку/заголовок) на Windows.
            const sf::Vector2u clampedClient =
                clampWindowedClientToWorkArea(desiredClient, workArea);

            const sf::VideoMode mode({clampedClient.x, clampedClient.y}, desktop.bitsPerPixel);

            window.create(mode, 
                          mTitle, 
                          sf::Style::Default, 
                          sf::State::Windowed,
                          srgbContextSettings);

            if (!window.isOpen()) {
                LOG_ERROR(core::log::cat::Engine,
                          "Failed to create Windowed window ({}x{}).",
                          mode.size.x,
                          mode.size.y);
                return false;
            }

            logSrgbAvailability();
            // Позиционирование:
            //  - если есть сохранённая позиция — клампим её в work area;
            //  - иначе — ставим в левый верх work area.
            const sf::Vector2i pos = clampWindowedPosition(desiredPos,
                                                           clampedClient,
                                                           workArea);

            window.setPosition(pos);

            mMode = newMode;
            return true;
        }
    }

    WindowModeManager::WorkArea WindowModeManager::getWorkAreaPrimary() noexcept {
#if defined(_WIN32)
        RECT rc{};
        if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &rc, 0) == TRUE) {
            const int w = static_cast<int>(rc.right - rc.left);
            const int h = static_cast<int>(rc.bottom - rc.top);
            if (w > 0 && h > 0) {
                return WorkArea{static_cast<int>(rc.left), static_cast<int>(rc.top), w, h};
            }
        }
#endif
        const sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
        return WorkArea{0, 0, static_cast<int>(desktop.size.x), static_cast<int>(desktop.size.y)};
    }

    sf::Vector2u WindowModeManager::clampWindowedClientToWorkArea(sf::Vector2u desiredClient,
                                                                  const WorkArea& workArea) noexcept {
        const std::uint32_t waW =
            (workArea.width > 0) ? static_cast<std::uint32_t>(workArea.width) : 1u;
        const std::uint32_t waH =
            (workArea.height > 0) ? static_cast<std::uint32_t>(workArea.height) : 1u;

#if defined(_WIN32)
        // Рассчитываем, сколько non-client добавит WS_OVERLAPPEDWINDOW.
        RECT rc{0, 0,
                static_cast<LONG>(std::max<std::uint32_t>(1u, desiredClient.x)),
                static_cast<LONG>(std::max<std::uint32_t>(1u, desiredClient.y))};

        const DWORD style = WS_OVERLAPPEDWINDOW;
        const DWORD exStyle = 0;

        if (AdjustWindowRectEx(&rc, style, FALSE, exStyle) != FALSE) {
            const int outerW = static_cast<int>(rc.right - rc.left);
            const int outerH = static_cast<int>(rc.bottom - rc.top);

            const int extraW = std::max(0, outerW - static_cast<int>(desiredClient.x));
            const int extraH = std::max(0, outerH - static_cast<int>(desiredClient.y));

            const std::uint32_t maxClientW =
                (waW > static_cast<std::uint32_t>(extraW))
                    ? (waW - static_cast<std::uint32_t>(extraW))
                    : 1u;
            const std::uint32_t maxClientH =
                (waH > static_cast<std::uint32_t>(extraH))
                    ? (waH - static_cast<std::uint32_t>(extraH))
                    : 1u;

            const bool fits =
                desiredClient.x <= maxClientW &&
                desiredClient.y <= maxClientH;

            if (fits) {
                return desiredClient;
            }

            // Сохраняем aspect: уменьшаем оба измерения одним коэффициентом.
            const double dw = static_cast<double>(std::max<std::uint32_t>(1u, desiredClient.x));
            const double dh = static_cast<double>(std::max<std::uint32_t>(1u, desiredClient.y));

            const double sx = static_cast<double>(maxClientW) / dw;
            const double sy = static_cast<double>(maxClientH) / dh;
            const double s = std::min(sx, sy);

            const std::uint32_t newW =
                std::max<std::uint32_t>(1u, static_cast<std::uint32_t>(dw * s));
            const std::uint32_t newH =
                std::max<std::uint32_t>(1u, static_cast<std::uint32_t>(dh * s));

            return sf::Vector2u{newW, newH};
        }
#endif

        // Fallback: clamp client напрямую (без учёта рамки).
        desiredClient.x = std::max<std::uint32_t>(1u, std::min(desiredClient.x, waW));
        desiredClient.y = std::max<std::uint32_t>(1u, std::min(desiredClient.y, waH));
        return desiredClient;
    }

    sf::Vector2i WindowModeManager::clampWindowedPosition(sf::Vector2i desiredPos,
                                                          sf::Vector2u clientSize,
                                                          const WorkArea& workArea) noexcept {
        int outerW = static_cast<int>(clientSize.x);
        int outerH = static_cast<int>(clientSize.y);

#if defined(_WIN32)
        RECT rc{0, 0, static_cast<LONG>(clientSize.x), static_cast<LONG>(clientSize.y)};
        if (AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, FALSE, 0) != FALSE) {
            outerW = static_cast<int>(rc.right - rc.left);
            outerH = static_cast<int>(rc.bottom - rc.top);
        }
#endif

        const int minX = workArea.left;
        const int minY = workArea.top;
        const int maxX = workArea.left + std::max(0, workArea.width - outerW);
        const int maxY = workArea.top + std::max(0, workArea.height - outerH);

        desiredPos.x = std::min(std::max(desiredPos.x, minX), maxX);
        desiredPos.y = std::min(std::max(desiredPos.y, minY), maxY);
        return desiredPos;
    }

} // namespace game::skyguard::presentation