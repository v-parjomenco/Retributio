#include "pch.h"

#include "core/ui/scaling_policy.h"

namespace core::ui {

    void UniformScalingPolicy::apply(sf::Sprite& sprite, const sf::View& view) {
        // Берём View изначального размера экрана
        sf::Vector2f baseViewSize{static_cast<float>(config::WINDOW_WIDTH),
                                  static_cast<float>(config::WINDOW_HEIGHT)};

        // Берём View текущего размера экрана, после изменения
        sf::Vector2f currentViewSize = view.getSize();

        // Вычисляем коэффициент, на который произошло изменение по каждой оси
        float scaleX = currentViewSize.x / baseViewSize.x;
        float scaleY = currentViewSize.y / baseViewSize.y;

        // Выбираем коэффициент, отражающий наименьшую деформацию сторон
        float newUniform = std::min(scaleX, scaleY);

        // Рассчитываем соотношение нового коэффициента к предыдущему
        float ratio = 1.f;        // изначально деформации не было, поэтому стартовое значение = 1.f
        if (mLastUniform > 0.f) { // проверка, чтобы избежать деления на 0
            // Cоотношение, насколько изменилось окно по сравнению с прошлым разом
            ratio = newUniform / mLastUniform;
        }

        // Получаем текущий коэффициент масштабирования спрайта
        sf::Vector2f currentScale = sprite.getScale();

        // Перемножаем старый коэффициент с новым и устанавливаем новый масштаб спрайта
        sprite.setScale({currentScale.x * ratio, currentScale.y * ratio});

        mLastUniform = newUniform;
    }

} // namespace core::ui