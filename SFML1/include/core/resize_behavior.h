#pragma once
#include <SFML/Graphics.hpp>
#include <memory>
#include "core/config.h"
#include "core/lock_policy.h"
#include "core/scaling_policy.h"

namespace core {

    // Виртуальный абстрактный родительский класс для изменения размеров окна
    class IResizeBehavior {
    public:
        virtual ~IResizeBehavior() = default;

        /**
         * @brief Метод вызывается при изменении окна или вида (view)
         * @param sprite Спрайт, для которого применяется поведение
         * @param view   Текущий вид (камера/экран)
         */
        virtual void onResize(sf::Sprite& sprite, const sf::View& view) = 0;
    };

    // Фиксирована позиция спрайта на мировых координатах, размер спрайта масштабируется
    class FixedWorldScalingBehavior final : public IResizeBehavior {
    public:
        void onResize(sf::Sprite& sprite, const sf::View& view) override;
    private:
        UniformScalingPolicy mScaling;
    };

    // Фиксирована позиция спрайта на мировых координатах, размер спрайта не масштабируется
    class FixedWorldNoScalingBehavior final : public IResizeBehavior {
    public:
        void onResize(sf::Sprite& /*sprite*/, const sf::View& /*view*/) override;
    };

    // Фиксирована позиция спрайта относительно экрана, размер спрайта масштабируется
    class FixedScreenScalingBehavior final : public IResizeBehavior {
    public:
        void onResize(sf::Sprite& sprite, const sf::View& view) override;

    private:
        UniformScalingPolicy mScaling;
        ScreenLockPolicy mLock;
    };

    // Фиксирована позиция спрайта относительно экрана, размер спрайта не масштабируется
    class FixedScreenNoScalingBehavior final : public IResizeBehavior {
    public:
        void onResize(sf::Sprite& sprite, const sf::View& view) override;
    private:
        ScreenLockPolicy mLock;
    };
} // namespace core