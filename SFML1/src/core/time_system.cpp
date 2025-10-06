#include "core/time_system.h"

namespace core {

    // Сброс таймера и обновление дельты времени и всех значений, зависящих от времени
    void TimeSystem::update(float smoothingAlpha) {
        // Перезапуск таймера — время с прошлого кадра 
        mDeltaTime = mClock.restart();

        // Мгновенный FPS (по времени между кадрами)
        mFps = (mDeltaTime.asSeconds() > 0.f) ? (1.f / mDeltaTime.asSeconds()) : 0.f;

        // Чтобы не было резкого скачка на первом кадре при старте с нуля
        if (mSmoothedFps == 0.f)
            mSmoothedFps = mFps; // первый кадр
        else
            mSmoothedFps = mSmoothedFps * (1.f - smoothingAlpha) + mFps * smoothingAlpha; // экспоненциальное сглаживание

        // Накопление времени для фиксированных апдейтов
        mTimeSinceLastUpdate += mDeltaTime;
    }

    // возвращает true, когда пора вызвать логику update() в игре (например, каждые 1/60 сек)
    bool TimeSystem::shouldUpdate(const sf::Time& fixedTimeStep) {
        if (mTimeSinceLastUpdate >= fixedTimeStep) {
            mTimeSinceLastUpdate -= fixedTimeStep;
            return true;
        }
        return false;
    }

    // Получение дельты времени
    sf::Time TimeSystem::getDeltaTime() const {
        return mDeltaTime;
    }

    // Получение дельты времени в секундах
    float TimeSystem::getDeltaTimeSeconds() const {
        return mDeltaTime.asSeconds();
    }

    // Получение мгновенного FPS
    float TimeSystem::getFps() const {
        return mFps;
    }

    // Получение сглаженного FPS
    float TimeSystem::getSmoothedFps() const {
        return mSmoothedFps;
    }
} // namespace core