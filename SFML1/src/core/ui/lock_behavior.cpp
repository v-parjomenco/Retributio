#include "pch.h"

#include "core/ui/lock_behavior.h"

namespace core::ui {

    void applyScreenLock(sf::Sprite& sprite, const sf::View& view, sf::Vector2f& previousViewSize,
                         bool& initialized) {

#ifndef NDEBUG
        // В Debug ожидаем корректную инициализацию в PlayerInitSystem:
        assert(previousViewSize.x > 0.f && previousViewSize.y > 0.f &&
               "LockBehavior: previousViewSize must be initialized before use");
#endif

        // Защита от деления на 0 (на случай некорректного состояния)
        const float previousWidth = std::max(1.f, previousViewSize.x);
        const float previousHeight = std::max(1.f, previousViewSize.y);

        // Предыдущая позиция спрайта (в пикселях на прошлом экране)
        const sf::Vector2f previousPosition = sprite.getPosition();

        // Относительное положение (в долях от старого размера)
        const sf::Vector2f relativePosition = {previousPosition.x / previousWidth,
                                               previousPosition.y / previousHeight};

        // Получаем размер экрана после масштабирования
        const sf::Vector2f currentViewSize = view.getSize();

        // Новая позиция с сохранением пропорций
        const sf::Vector2f newPosition = {currentViewSize.x * relativePosition.x,
                                          currentViewSize.y * relativePosition.y};

        sprite.setPosition(newPosition);

        // Обновляем сохранённый размер экрана для следующего вызова
        previousViewSize = currentViewSize;
        initialized = true;
    }

} // namespace core::ui