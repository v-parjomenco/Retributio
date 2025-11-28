#include "pch.h"

#include <algorithm>
#include <cmath>
#include <type_traits>

#include "core/time/time_service.h"

namespace core::time {

    // Гарантируем, что TimeService можно безопасно перемещать без исключений
    static_assert(std::is_nothrow_move_constructible_v<TimeService>);
    static_assert(std::is_nothrow_move_assignable_v<TimeService>);

    void TimeService::tick(float smoothingAlpha) noexcept {
        // 1. Измеряем "сырое" dt с прошлого кадра
        sf::Time raw = mClock.restart();
        float rawSec = raw.asSeconds();

        // Защита от NaN/Inf и отрицательного времени
        if (!std::isfinite(rawSec) || rawSec < 0.f) {
            rawSec = 0.f;
        }

        // Ограничение максимального dt (паузы, сворачивание, лаги ОС)
        rawSec = std::clamp(rawSec, 0.f, kMaxDeltaSeconds);

        mRawDeltaTime = sf::seconds(rawSec);

        // 2. Мгновенный FPS по raw dt
        mFps = (rawSec > 0.f) ? (1.f / rawSec) : 0.f;

        // Счётчик кадров и min/max FPS (если FPS > 0)
        ++mFrameCount;
        if (mFps > 0.f) {
            if (mFps < mMinFps) {
                mMinFps = mFps;
            }
            if (mFps > mMaxFps) {
                mMaxFps = mFps;
            }
        }

        // 3. Сглаженный FPS (экспоненциальное сглаживание)
        if (!mInitialized) {
            mSmoothedFps = mFps;
            mInitialized = true;
        } else {
            const float alpha = std::clamp(smoothingAlpha, 0.f, 1.f);
            mSmoothedFps += (mFps - mSmoothedFps) * alpha;
        }

        // 4. Применяем паузу и масштаб времени
        const float scaledSec = (mPaused || mTimeScale <= 0.f) ? 0.f : (rawSec * mTimeScale);

        mDeltaTime = sf::seconds(scaledSec);

        // 5. Накопление "симуляционного" dt для fixed-step с ограничением
        const float accumulatedSec =
            std::min((mTimeSinceLastUpdate + mDeltaTime).asSeconds(), kMaxAccumulatedSeconds);

        mTimeSinceLastUpdate = sf::seconds(accumulatedSec);
    }

    // Возвращает true, когда пора вызвать один шаг фиксированной логики.
    bool TimeService::shouldUpdate(const sf::Time& fixedTimeStep) noexcept {
        if (mTimeSinceLastUpdate >= fixedTimeStep) {
            mTimeSinceLastUpdate -= fixedTimeStep;
            return true;
        }
        return false;
    }

} // namespace core::time
