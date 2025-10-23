#pragma once

#include <SFML/Graphics.hpp>
#include "core/config.h"
#include <algorithm>

namespace core {

    /**
     * @brief Интерфейс фиксации позиции на экране
     */
    class ILockPolicy {
    public:
        virtual ~ILockPolicy() = default;
        virtual void apply(sf::Sprite& sprite, const sf::View& view) = 0;
    };

    /**
     * @brief Фиксация позиции относительно экрана
     */
    class ScreenLockPolicy final : public ILockPolicy {
    public:
        void apply(sf::Sprite& sprite, const sf::View& view) override;

    private:
        sf::Vector2f mPreviousViewSize;
        bool mInitialized{ false };
    };

} // namespace core