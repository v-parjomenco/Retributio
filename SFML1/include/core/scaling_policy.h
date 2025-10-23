#pragma once

#include <SFML/Graphics.hpp>
#include "core/config.h"

namespace core {

    /**
     * @brief Интерфейс политики масштабирования
     */
    class IScalingPolicy {
    public:
        virtual ~IScalingPolicy() = default;
        virtual void apply(sf::Sprite& sprite, const sf::View& view) = 0;
    };

    /**
     * @brief Масштабирование спрайта при изменении окна
     */
    class UniformScalingPolicy final : public IScalingPolicy {
    public:
        void apply(sf::Sprite& sprite, const sf::View& view) override;
    private:
        float mLastUniform{ 1.f };
    };

} // namespace core