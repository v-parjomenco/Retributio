#include "pch.h"

#include "core/ui/scaling_behavior.h"

namespace core::ui {

    void applyUniformScaling(sf::Sprite& sprite, const sf::View& view,
                             const sf::Vector2f& baseViewSize, float& lastUniform) {

        // Защита от некорректной инициализации baseViewSize
        const sf::Vector2f safeBaseSize{std::max(baseViewSize.x, 1.f),
                                        std::max(baseViewSize.y, 1.f)};

        // Текущий размер view после изменения
        const sf::Vector2f currentViewSize = view.getSize();

        // Коэффициенты изменения по осям
        const float scaleX = currentViewSize.x / safeBaseSize.x;
        const float scaleY = currentViewSize.y / safeBaseSize.y;

        // Равномерный масштаб
        const float newUniform = std::min(scaleX, scaleY);

        // Относительное изменение по сравнению с предыдущим uniform
        float ratio = 1.f;
        if (lastUniform > 0.f) {
            ratio = newUniform / lastUniform;
        }

        // Применяем изменение к текущему масштабу спрайта
        const sf::Vector2f currentScale = sprite.getScale();
        sprite.setScale({currentScale.x * ratio, currentScale.y * ratio});

        // Сохраняем новый uniform для следующего resize
        lastUniform = newUniform;
    }

} // namespace core::ui