#pragma once

#include <SFML/System/Clock.hpp>
#include <SFML/System/Time.hpp>

namespace core {

    class TimeSystem {
    public:
        // Сброс таймера и обновление дельты времени и всех значений, зависящих от времени
        void update(float smoothingAlpha = 0.1f);

        // Возвращает true, когда пора вызвать логику update() в игре (например, каждые 1/60 сек)
        bool shouldUpdate(const sf::Time& fixedTimeStep);

        // Получение дельты времени
        sf::Time getDeltaTime() const;

        // Получение дельты времени в секундах
        float getDeltaTimeSeconds() const;

        // Получение мгновенного FPS
        float getFps() const;

        // Получение сглаженного FPS
        float getSmoothedFps() const;

    private:
        sf::Clock mClock; // таймер
        sf::Time mDeltaTime; // дельта времени - время, прошедшее с прошлого кадра 
        float mFps{ 0.f }; // frames per second
        float mSmoothedFps{ 0.f }; // сглаженный frames per second
        sf::Time mTimeSinceLastUpdate = sf::Time::Zero;
    };
} // namespace core