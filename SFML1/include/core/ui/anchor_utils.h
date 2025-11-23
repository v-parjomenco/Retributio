// ================================================================================================
// File: core/ui/anchor_utils.h
// Purpose: AnchorType enum and helpers for anchor-based positioning
// Used by: AnchorProperties, AnchorPolicy, PlayerInitSystem, UI/layout systems
// Related headers: core/ui/ids/ui_id_utils.h (string <-> enum helpers), core/config.h
// ================================================================================================

#pragma once

#include <cassert>
#include <cstdint>

#include <SFML/System/Vector2.hpp>

namespace core::ui {

    /**
     * @brief Тип якоря на экране.
     *
     * Используется для привязки объекта к определённой части экрана:
     *  - TopLeft / TopCenter / TopRight
     *  - CenterLeft / Center / CenterRight
     *  - BottomLeft / BottomCenter / BottomRight
     *  - None — не использовать якорь, работать в мировых координатах.
     *
     * ВАЖНО:
     *  - Строковые имена ("BottomCenter" и т.п.) живут в core::ui::ids::toString(...)
     *    и сопоставляются через core::ui::ids::anchorFromString(...).
     *  - Этот enum вообще не знает о JSON/строках.
     */
    enum class AnchorType : std::uint8_t {
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
         * @brief Вычисляет смещение якоря относительно размера прямоугольника.
         *
         * size — размер объекта (например, спрайта) в пикселях.
         * type — якорь, указывающий, к какой части объекта привязываться.
         *
         * Примеры:
         *  - TopLeft      -> (0, 0)
         *  - Center       -> (size.x * 0.5f, size.y * 0.5f)
         *  - BottomRight  -> (size.x, size.y)
         *
         * None -> (0, 0).
         */
        inline sf::Vector2f computeAnchorOffset(const sf::Vector2f& size, AnchorType type) {
            switch (type) {
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
                return {0.f, 0.f};
            default:
                // Защита от потенциально некорректного значения enum
                assert(false && "Unknown AnchorType in computeAnchorOffset");
                return {0.f, 0.f};
            }
        }

        // ВНИМАНИЕ:
        //  - Конвертация string <-> AnchorType живёт в core::ui::ids
        //    (см. core/ui/ids/ui_id_utils.h / .cpp).
        //  - Здесь остаётся только чистая математика, без JSON и строк.

    } // namespace anchors
} // namespace core::ui