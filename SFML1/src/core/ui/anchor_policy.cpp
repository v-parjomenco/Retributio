#include "core/ui/anchor_policy.h"

#include "core/ui/anchor_utils.h"

namespace core::ui {

    void ui::AnchorPolicy::apply(sf::Sprite& sprite, const sf::View& view) const {
        // Получаем локальные границы (без учёта scale и transform)
        sf::FloatRect bounds = sprite.getLocalBounds();

        // Вычисляем origin на основе выбранного якоря
        sf::Vector2f origin = anchors::computeAnchorOffset({bounds.size.x, bounds.size.y}, mAnchor);
        sprite.setOrigin(origin);

        // Теперь вычисляем позицию спрайта на экране (view)
        const sf::Vector2f viewSize = view.getSize();

        // Вычисляем screen position (от верхнего-левого угла видимой области)
        sf::Vector2f screenPosition;

        switch (mAnchor) {
        case AnchorType::TopLeft:
            screenPosition = {0.f, 0.f};
            break;
        case AnchorType::TopCenter:
            screenPosition = {viewSize.x * 0.5f, 0.f};
            break;
        case AnchorType::TopRight:
            screenPosition = {viewSize.x, 0.f};
            break;
        case AnchorType::CenterLeft:
            screenPosition = {0.f, viewSize.y * 0.5f};
            break;
        case AnchorType::Center:
            screenPosition = {viewSize.x * 0.5f, viewSize.y * 0.5f};
            break;
        case AnchorType::CenterRight:
            screenPosition = {viewSize.x, viewSize.y * 0.5f};
            break;
        case AnchorType::BottomLeft:
            screenPosition = {0.f, viewSize.y};
            break;
        case AnchorType::BottomCenter:
            screenPosition = {viewSize.x * 0.5f, viewSize.y};
            break;
        case AnchorType::BottomRight:
            screenPosition = {viewSize.x, viewSize.y};
            break;
        case AnchorType::None:
        default:
            // ничего не меняем — оставляем текущую позицию
            return;
        }

        // Конвертируем screenPosition (от top-left видимой области) -> мировые координаты:
        // viewTopLeft = view.center - view.size/2
        const sf::Vector2f viewCenter = view.getCenter();
        const sf::Vector2f viewTopLeft = {viewCenter.x - viewSize.x / 2.f,
                                          viewCenter.y - viewSize.y / 2.f};

        const sf::Vector2f worldPosition = viewTopLeft + screenPosition;

        // Устанавливаем позицию спрайта в мировые координаты так, чтобы
        // локальная точка origin оказалась в worldPosition
        sprite.setPosition(
            worldPosition); // setPosition принимает позицию, где будет размещена точка origin
    }

} // namespace core::ui