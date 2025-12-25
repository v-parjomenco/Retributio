// ================================================================================================
// File: core/ui/anchor_policy.h
// Purpose: Anchor policy for sprite origin/position
// Used by: PlayerInitSystem, future UI/layout systems
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