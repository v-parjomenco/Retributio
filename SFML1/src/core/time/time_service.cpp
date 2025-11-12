#include "core/time/time_service.h"

namespace core::time {

    void TimeService::tick(float smoothingAlpha) noexcept {
        // Перезапуск таймера — получаем dt предыдущего кадра
        mDeltaTime = mClock.restart();

        // Мгновенный FPS
        const float dtSec = mDeltaTime.asSeconds();
        mFps = (dtSec > 0.f) ? (1.f / dtSec) : 0.f;

        // Экспоненциальное сглаживание
        // Чтобы не было резкого скачка на первом кадре при старте с нуля
        if (!mInitialized) {
            mSmoothedFps = mFps;
            mInitialized = true;
        } else {
            mSmoothedFps = mSmoothedFps * (1.f - smoothingAlpha) + mFps * smoothingAlpha;
        }

        // Накопление времени для фиксированных апдейтов
        mTimeSinceLastUpdate += mDeltaTime;
    }

    // Возвращает true, когда пора вызвать логику update() в игре (например, каждые 1/60 сек)
    bool TimeService::shouldUpdate(const sf::Time& fixedTimeStep) noexcept {
        if (mTimeSinceLastUpdate >= fixedTimeStep) {
            mTimeSinceLastUpdate -= fixedTimeStep;
            return true;
        }
        return false;
    }

} // namespace core::time