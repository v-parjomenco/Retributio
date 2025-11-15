#pragma once

#include <algorithm>

#include <SFML/Graphics.hpp>

#include "core/config.h"

namespace core::ui {

    /**
     * @brief Интерфейс фиксации позиции на экране
     */
    class ILockPolicy {
      public:
        virtual ~ILockPolicy() = default;
        virtual void apply(sf::Sprite& sprite, const sf::View& view) = 0;
    };

    /**
     * @brief Политика фиксации позиции спрайта относительно экрана (не мира).
     *
     * Ожидается, что позиция спрайта на момент первого вызова apply() задана в экранных координатах — 
     * например, после применения AnchorPolicy.
     *
     * Если спрайт был создан в мировых координатах, поведение может быть некорректным.
     */
    class ScreenLockPolicy final : public ILockPolicy {
      public:
        void apply(sf::Sprite& sprite, const sf::View& view) override;

      private:
        sf::Vector2f mPreviousViewSize;
        bool mInitialized{false};
    };

} // namespace core::ui