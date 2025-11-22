#include "pch.h"

#include "core/ui/lock_behavior.h"
#include "core/config.h"
#include "core/utils/message.h"

namespace core::ui {

    LockBehaviorKind lockBehaviorFromString(
        std::string_view name,
        LockBehaviorKind defaultKind) {
            if (name == "ScreenLock") {
                return LockBehaviorKind::Screen;
            }
            if (name == "WorldLock") {
                return LockBehaviorKind::World;
            }

            // Сообщаем об ошибке, но не валим игру: сбрасываем на defaultKind.
            core::utils::message::showError(
                std::string("[LockBehavior]\nНеизвестное значение lock_behavior: ") +
                std::string{name} + ". Применено значение по умолчанию.");

            return defaultKind;
    }

    void applyScreenLock(sf::Sprite& sprite,
                         const sf::View& view,
                         sf::Vector2f& previousViewSize,
                         bool& initialized) {

        // При первом вызове используем базовый размер из config.h
        if (!initialized) {
            previousViewSize = {static_cast<float>(config::WINDOW_WIDTH),
                                static_cast<float>(config::WINDOW_HEIGHT)};
            initialized = true;
        }

        // Защита от деления на 0 (на случай старта с нулевым размером окна)
        const float previousWidth  = std::max(1.f, previousViewSize.x);
        const float previousHeight = std::max(1.f, previousViewSize.y);

        // Предыдущая позиция спрайта (в пикселях на прошлом экране)
        const sf::Vector2f previousPosition = sprite.getPosition();

        // Относительное положение (в долях от старого размера)
        const sf::Vector2f relativePosition = {
            previousPosition.x / previousWidth,
            previousPosition.y / previousHeight};

        // Получаем размер экрана после масштабирования
        const sf::Vector2f currentViewSize = view.getSize();

        // Пересчитываем новую позицию, сохраняя пропорции на экране
        const sf::Vector2f newPosition = {
            currentViewSize.x * relativePosition.x,
            currentViewSize.y * relativePosition.y};

        sprite.setPosition(newPosition);

        // Обновляем сохранённый размер экрана для следующего вызова
        previousViewSize = currentViewSize;
    }

} // namespace core::ui