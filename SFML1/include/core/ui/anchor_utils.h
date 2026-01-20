// ================================================================================================
// File: core/ui/anchor_utils.h
// Purpose: Anchor type definitions and utility helpers (no ownership, no global state)
// Used by: Optional UI/layout bricks (AnchorPolicy, AnchorProperties), tools/editors
// Notes:
//  - Not used by SkyGuard. Kept for potential usage in future games/tools.
// ================================================================================================
#pragma once

#include <SFML/Graphics/View.hpp>
#include <SFML/System/Vector2.hpp>

namespace core::ui {

    /**
     * @brief Типы якорей (anchor points) для UI-элементов и спрайтов.
     *
     * Определяет 9 стандартных точек привязки + None (без привязки).
     */
    enum class AnchorType {
        None,
        TopLeft,
        TopCenter,
        TopRight,
        CenterLeft,
        Center,
        CenterRight,
        BottomLeft,
        BottomCenter,
        BottomRight
    };

    namespace anchors {

        /**
         * @brief Вычисляет смещение origin для заданного anchor type.
         *
         * @param size Размер объекта (локальные bounds спрайта или UI-элемента)
         * @param anchor Тип якоря
         * @return Смещение origin относительно top-left угла объекта
         *
         * Пример:
         *   computeAnchorOffset({100, 50}, AnchorType::Center) → {50, 25}
         */
        inline sf::Vector2f computeAnchorOffset(const sf::Vector2f& size, AnchorType anchor) {
            switch (anchor) {
            case AnchorType::TopLeft:
                return {0.f, 0.f};
            case AnchorType::TopCenter:
                return {size.x * 0.5f, 0.f};
            case AnchorType::TopRight:
                return {size.x, 0.f};
            case AnchorType::CenterLeft:
                return {0.f, size.y * 0.5f};
            case AnchorType::Center:
                return {size.x * 0.5f, size.y * 0.5f};
            case AnchorType::CenterRight:
                return {size.x, size.y * 0.5f};
            case AnchorType::BottomLeft:
                return {0.f, size.y};
            case AnchorType::BottomCenter:
                return {size.x * 0.5f, size.y};
            case AnchorType::BottomRight:
                return {size.x, size.y};
            case AnchorType::None:
            default:
                return {0.f, 0.f};
            }
        }

        // ========================================================================================
        // DATA-ONLY HELPERS (no sf::Sprite mutation)
        // ========================================================================================

        /**
         * @brief Результат вычисления anchor данных (data-driven, no sf::Sprite mutation).
         */
        struct AnchorData {
            sf::Vector2f origin;   // origin для sf::Sprite::setOrigin()
            sf::Vector2f position; // мировая позиция для TransformComponent
        };

        /**
         * @brief Data-only anchor computation (no sf::Sprite mutation).
         *
         * Вычисляет origin и world position БЕЗ мутации sf::Sprite.
         *
         * Старый подход (sprite-based):
         *   AnchorPolicy::apply(sf::Sprite& sprite, view) {
         *       sprite.setOrigin(...);
         *       sprite.setPosition(...);
         *   }
         *
         * Новый подход (data-driven):
         *   auto [origin, position] = computeAnchorData(textureSize, scale, anchor, view);
         *   spriteComp.origin = origin;
         *   transformComp.position = position;
         *
         * @param textureSize Размер текстуры в пикселях
         * @param scale Текущий масштаб спрайта
         * @param anchor Тип якоря
         * @param view SFML view (для вычисления screen position)
         * @return {origin, position}
         */
        inline AnchorData computeAnchorData(const sf::Vector2u& textureSize,
                                            const sf::Vector2f& scale, AnchorType anchor,
                                            const sf::View& view) {
            (void) scale; // Reserved for future use (sprite bounds calculation with scale)

            // Локальные границы (как sprite.getLocalBounds())
            const sf::Vector2f localSize(static_cast<float>(textureSize.x),
                                         static_cast<float>(textureSize.y));

            // Вычисляем origin (смещение точки привязки)
            const sf::Vector2f origin = computeAnchorOffset(localSize, anchor);

            // Вычисляем screen position (от top-left видимой области)
            const sf::Vector2f viewSize = view.getSize();
            sf::Vector2f screenPosition;

            switch (anchor) {
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
                // None = не применяем anchor, возвращаем нулевые данные
                return {{0.f, 0.f}, {0.f, 0.f}};
            }

            // Конвертируем screen position → world position
            const sf::Vector2f viewCenter = view.getCenter();
            const sf::Vector2f viewTopLeft = {viewCenter.x - viewSize.x / 2.f,
                                              viewCenter.y - viewSize.y / 2.f};
            const sf::Vector2f worldPosition = viewTopLeft + screenPosition;

            return {origin, worldPosition};
        }

    } // namespace anchors

} // namespace core::ui