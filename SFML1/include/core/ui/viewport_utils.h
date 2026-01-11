// ================================================================================================
// File: core/ui/viewport_utils.h
// Purpose: Viewport helpers for letterbox/pillarbox calculations.
// Used by: game layer view managers and presentation code.
// Related headers: none (header-only utility).
// ================================================================================================
#pragma once

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

namespace core::ui {

    /**
     * @brief Вычислить letterbox/pillarbox viewport для фиксированного логического размера.
     *
     * Сохраняет соотношение сторон logicalSize внутри windowSize.
     * Возвращает viewport в нормализованных координатах [0, 1].
     *
     * @param windowSize Текущий размер окна в пикселях
     * @param logicalSize Фиксированный логический размер (world or UI units)
     * @return Viewport rect для sf::View::setViewport()
     *
     * @note Fallback {0,0,1,1} НЕ сохраняет соотношение сторон.
     *       Вызывающая сторона должна гарантировать валидность размеров окна.
     */
    [[nodiscard]] inline sf::FloatRect computeLetterboxViewport(
        const sf::Vector2u& windowSize,
        const sf::Vector2f& logicalSize) noexcept {

        // ВАЖНО: фолбэк НЕ сохраняет соотношение сторон.
        // Это защитная мера для невалидного ввода, не штатный режим.
        if (windowSize.x == 0 || windowSize.y == 0 ||
            logicalSize.x <= 0.f || logicalSize.y <= 0.f) {
            return sf::FloatRect({0.f, 0.f},
                                 {1.f, 1.f});
        }

        const float windowAspect = static_cast<float>(windowSize.x) /
                                   static_cast<float>(windowSize.y);
        const float logicalAspect = logicalSize.x / logicalSize.y;

        float viewportWidth = 1.f;
        float viewportHeight = 1.f;
        float viewportX = 0.f;
        float viewportY = 0.f;

        if (windowAspect > logicalAspect) {
            // Если окно шире логического → pillarbox (black bars on sides)
            viewportWidth = logicalAspect / windowAspect;
            viewportX = (1.f - viewportWidth) * 0.5f;
        } else {
            // Если окно выше логического → letterbox (black bars top/bottom)
            viewportHeight = windowAspect / logicalAspect;
            viewportY = (1.f - viewportHeight) * 0.5f;
        }

            return sf::FloatRect({viewportX, viewportY},
                                 {viewportWidth, viewportHeight});
    }

} // namespace core::ui