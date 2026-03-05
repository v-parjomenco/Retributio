// ================================================================================================
// File: core/ui/anchor_policy.h
// Purpose: Sprite-based anchor helper (mutates sf::Sprite origin/position relative to a View)
// Used by: Optional UI/prototyping code and tools (non-ECS)
// Notes:
//  - Not used by Atrapacielos. Kept for potential usage in future games/tools.
// ================================================================================================
#pragma once

#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/View.hpp>

#include "core/ui/anchor_utils.h"

namespace core::ui {

    /**
     * @brief Политика установки якоря для спрайта.
     *
     * Отвечает за вычисление origin и начальной позиции на экране.
     * Не знает ничего о JSON, конфиге или игре.
     */
    class AnchorPolicy final {
      public:
        explicit AnchorPolicy(AnchorType anchor = AnchorType::None) : mAnchor(anchor) {
        }

        void apply(sf::Sprite& sprite, const sf::View& view) const;

        AnchorType getAnchor() const {
            return mAnchor;
        }

        void setAnchor(AnchorType anchor) {
            mAnchor = anchor;
        }

      private:
        AnchorType mAnchor{AnchorType::None};
    };

} // namespace core::ui