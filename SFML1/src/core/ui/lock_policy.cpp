#include "pch.h"

#include "core/ui/lock_policy.h"

namespace core::ui {

    void ui::ScreenLockPolicy::apply(sf::Sprite& sprite, const sf::View& view) {

        // При первом вызове используем базовый размер из config.h
        if (!mInitialized) {
            mPreviousViewSize = {static_cast<float>(config::WINDOW_WIDTH),
                                 static_cast<float>(config::WINDOW_HEIGHT)};
            mInitialized = true;
        }

        // Защита от деления на 0 (на случай старта с нулевым размером окна)
        const float previousWidth = std::max(1.f, mPreviousViewSize.x);
        const float previousHeight = std::max(1.f, mPreviousViewSize.y);

        // Предыдущая позиция спрайта (в пикселях на прошлом экране)
        const sf::Vector2f previousPosition = sprite.getPosition();

        // Относительное положение (в долях от старого размера)
        const sf::Vector2f relativePosition = {previousPosition.x / previousWidth,
                                               previousPosition.y / previousHeight};

        // Получаем размер экрана после масштабирования
        const sf::Vector2f currentViewSize = view.getSize();

        // Пересчитываем новую позицию, сохраняя пропорции на экране
        const sf::Vector2f newPosition = {currentViewSize.x * relativePosition.x,
                                          currentViewSize.y * relativePosition.y};

        sprite.setPosition(newPosition);

        // Обновляем сохранённый размер экрана для следующего вызова
        mPreviousViewSize = currentViewSize;
    }

} // namespace core::ui