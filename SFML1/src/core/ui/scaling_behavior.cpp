#include "pch.h"

#include "core/ui/scaling_behavior.h"

#include "core/config.h"

namespace core::ui {

    void applyUniformScaling(sf::Sprite& sprite, const sf::View& view, float& lastUniform) {

        // Берём View изначального размера экрана
        sf::Vector2f baseViewSize{static_cast<float>(config::WINDOW_WIDTH),
                                  static_cast<float>(config::WINDOW_HEIGHT)};

        // Берём View текущего размера экрана, после изменения
        sf::Vector2f currentViewSize = view.getSize();

        // Вычисляем коэффициент, на который произошло изменение по каждой оси
        const float scaleX = currentViewSize.x / baseViewSize.x;
        const float scaleY = currentViewSize.y / baseViewSize.y;

        // Выбираем коэффициент, отражающий наименьшую деформацию сторон
        const float newUniform = std::min(scaleX, scaleY);

        // Рассчитываем соотношение нового коэффициента к предыдущему
        float ratio = 1.f;       // изначально деформации не было
        if (lastUniform > 0.f) { // защита от деления на 0
            ratio = newUniform / lastUniform;
        }

        // Получаем текущий коэффициент масштабирования спрайта
        const sf::Vector2f currentScale = sprite.getScale();

        // Перемножаем старый коэффициент с новым и устанавливаем новый масштаб спрайта
        sprite.setScale({currentScale.x * ratio, currentScale.y * ratio});

        // Обновляем сохранённое значение для следующего resize
        lastUniform = newUniform;
    }

} // namespace core::ui