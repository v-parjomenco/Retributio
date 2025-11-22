// ================================================================================================
// File: core/config/properties/text_properties.h
// Purpose: Reusable text rendering properties (position, size, color)
// Used by: DebugOverlayBlueprint, future UI/text blueprints
// Related headers: (none, generic data brick)
// ================================================================================================
#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>

namespace core::config::properties {

    /**
 * @brief Набор свойств для отрисовки текста.
 *
 * Здесь описываются только данные:
 *  - позиция текста на экране;
 *  - размер шрифта;
 *  - цвет.
 *
 * Логика отрисовки и выбор шрифта живут в других модулях (системах/виджетах).
 */
    struct TextProperties {
        sf::Vector2f position{10.f, 10.f}; // позиция текста в мировых координатах / пикселях
        unsigned int characterSize = 35;   // размер шрифта
        sf::Color color{255, 0, 0, 255};   // цвет текста по умолчанию (красный)
    };

} // namespace core::config::properties