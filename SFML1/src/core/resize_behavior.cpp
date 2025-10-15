#include <algorithm>
#include <SFML/Graphics.hpp>
#include "core/config.h"
#include "core/resize_behavior.h"

namespace core {

    // Фиксирована позиция спрайта на мировых координатах, размер спрайта масштабируется
    void FixedWorldScalingBehavior::onResize(sf::Sprite& sprite, const sf::View& view) {
        mScaling.apply(sprite, view);
    }

    // Фиксирована позиция спрайта на мировых координатах, размер спрайта не масштабируется
    void FixedWorldNoScalingBehavior::onResize(sf::Sprite& /*sprite*/, const sf::View& /*view*/) {
        // Ничего не делаем - объект остаётся в мировых координатах, его scale и position не меняются
    }

    // Фиксирована позиция спрайта относительно экрана, размер спрайта масштабируется
    void FixedScreenScalingBehavior::onResize(sf::Sprite& sprite, const sf::View& view) {
        mScaling.apply(sprite, view);
        mLock.apply(sprite, view);
    }

    // Фиксирована позиция спрайта относительно экрана, размер спрайта не масштабируется
    void FixedScreenNoScalingBehavior::onResize(sf::Sprite& sprite, const sf::View& view) {
        mLock.apply(sprite, view);
    }

} // namespace core