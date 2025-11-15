// ============================================================================
// File: core/time/time_service.h
// Role: Central frame timing service for the game loop (outside ECS)
// Owned by: Game (engine/core layer)
// Notes:
//  - Provides tick(), fixed-step gating, and FPS (instant & smoothed)
//  - ECS systems receive dt as a parameter; they don't own timing
//  - This matches common AAA engine practice (deterministic systems)
// ============================================================================

#pragma once
#include <SFML/System/Clock.hpp>
#include <SFML/System/Time.hpp>

namespace core::time {

    class TimeService {
      public:
        // Перезапуск всех временных метрик (используется при паузе/рестарте сцены)
        void reset() noexcept {
            mClock.restart();
            mDeltaTime = sf::Time::Zero;
            mFps = 0.f;
            mSmoothedFps = 0.f;
            mTimeSinceLastUpdate = sf::Time::Zero;
            mInitialized = false;
        }

        // Обновить таймеры — вызывается 1 раз за кадр в Game::run()
        void tick(float smoothingAlpha = 0.1f) noexcept;

        // true, если накопилось достаточно времени для одного фиксированного шага
        [[nodiscard]] bool shouldUpdate(const sf::Time& fixedTimeStep) noexcept;

        // Доступ к метрикам времени
        [[nodiscard]] sf::Time getDeltaTime() const noexcept {
            return mDeltaTime;
        }
        [[nodiscard]] float getDeltaTimeSeconds() const noexcept {
            return mDeltaTime.asSeconds();
        }
        [[nodiscard]] float getFps() const noexcept {
            return mFps;
        }
        [[nodiscard]] float getSmoothedFps() const noexcept {
            return mSmoothedFps;
        }

      private:
        sf::Clock mClock;                // секундомер кадра
        sf::Time mDeltaTime;             // дельта времени между кадрами
        float mFps{0.f};                 // мгновенный FPS
        float mSmoothedFps{0.f};         // сглаженный FPS
        sf::Time mTimeSinceLastUpdate{}; // накопитель для fixed-step
        bool mInitialized{false};        // чтобы избежать скачка на первом кадре
    };

} // namespace core::time