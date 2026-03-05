// ================================================================================================
// File: core/time/time_service.h
// Role: Central frame timing service for the game loop (outside ECS)
// Owned by: Game (engine/core layer)
// Notes:
//  - Provides tick(), fixed-step gating, and FPS (instant & smoothed)
//  - ECS systems receive dt as a parameter; they don't own timing
//  - This matches common AAA engine practice (deterministic systems)
// ================================================================================================
#pragma once

#include <cstdint>
#include <limits>

#include <SFML/System/Clock.hpp>
#include <SFML/System/Time.hpp>

namespace core::time {

    /**
     * @brief Центральный сервис времени для игрового цикла.
     *
     * Инварианты и философия:
     *  - TimeService вызывается ВНЕ ECS (игровым циклом / Game);
     *  - ECS-системы получают dt как параметр, но не управляют временем;
     *  - отделяем:
     *      * "сырое" время кадра (raw dt, FPS) — как реально идёт рендер,
     *      * "симуляционное" время (scaled dt) — с учётом паузы и timeScale;
     *  - fixed-step логика использует ИМЕННО scaled dt.
     *
     * Предназначение:
     *  - как казуальные игры (Atrapacielos), так и большие симуляции (4X, сотни тысяч сущностей);
     *  - поддержка:
     *      * паузы,
     *      * замедления / ускорения времени (timeScale),
     *      * сглаженного FPS для DebugOverlaySystem.
     *
     * Важно:
     *  - tick() нужно вызывать ровно 1 раз за кадр;
     *  - shouldUpdate(fixedTimeStep) нужно вызывать в игровом цикле (обычно в while-цикле),
     *    чтобы отдать один или несколько fixed-шагов.
     *
     * Замечание про большие timeScale:
     *  - при очень больших значениях (например, x50, x100) число fixed-апдейтов за один кадр
     *    рендера может стать большим;
     *  - это не "сжигает CPU" физически, но даёт высокую нагрузку и может привести к тому, что
     *    игра начнёт тратить много времени на логику между кадрами рендеринга;
     *  - на текущем этапе мы выбираем честную симуляцию (детерминизм), а более агрессивные
     *    оптимизации (LOD по времени, динамическое изменение fixedTimeStep, лимиты апдейтов за
     *    кадр) будут реализовываться позже НАД TimeService, на уровне игрового цикла / 4X-логики,
     *    а не внутри этого класса.
     *
     * Также:
     *  - более сложные механизмы (несколько "слоёв" времени для геймплея/UI/эффектов, callbacks
     *    на lag и т.п.) сознательно не вшиваются сюда, чтобы TimeService оставался простым и
     *    универсальным кирпичом движка.
     */
    class TimeService {
      public:
        // ----------------------------------------------------------------------------------------
        // Public константы для вычисления game-level политики
        // ----------------------------------------------------------------------------------------
        
        /// @brief Максимальное накопленное время для fixed-step (секунды).
        ///
        /// Используется:
        ///  - внутри TimeService для clamp накопления (защита от spiral of death);
        ///  - в Game::run() для вычисления максимального числа апдейтов за кадр:
        ///      maxUpdates = kMaxAccumulatedSeconds / FIXED_TIME_STEP
        ///
        /// Архитектура:
        ///  - TimeService управляет временем (константы, накопление);
        ///  - Game управляет политикой (количество апдейтов), но выводит её из TimeService.
        static constexpr float kMaxAccumulatedSeconds = 0.5f;

        // ----------------------------------------------------------------------------------------
        // Публичный API
        // ----------------------------------------------------------------------------------------
        
        TimeService() = default;

        /**
         * @brief Полный сброс всех временных метрик.
         *
         * Используется при:
         *  - паузе / продолжении игры по кнопке "Restart";
         *  - загрузке новой сцены / уровня.
         */
        void reset() noexcept {
            mClock.restart();
            mRawDeltaTime = sf::Time::Zero;
            mDeltaTime = sf::Time::Zero;
            mTimeSinceLastUpdate = sf::Time::Zero;
            mFps = 0.f;
            mSmoothedFps = 0.f;
            mInitialized = false;
            mPaused = false;
            mTimeScale = 1.f;
            mFrameCount = 0;
            mMinFps = std::numeric_limits<float>::max();
            mMaxFps = 0.f;
        }

        /**
         * @brief Обновить таймеры — вызывается 1 раз за кадр в Game::run().
         *
         * @param smoothingAlpha коэффициент экспоненциального сглаживания FPS [0;1].
         *
         * Поведение:
         *  - измеряет "сырое" dt с момента прошлого кадра;
         *  - защищает от NaN/Inf и слишком больших значений dt;
         *  - считает мгновенный FPS и сглаженный FPS по raw dt;
         *  - применяет pause / timeScale и вычисляет "симуляционный" dt;
         *  - накапливает "симуляционный" dt для fixed-step обновлений;
         *  - собирает базовые метрики (frameCount, min/max FPS).
         */
        void tick(float smoothingAlpha = 0.1f) noexcept;

        /**
         * @brief true, если накопилось достаточно времени для одного фиксированного шага.
         *
         * Протокол использования в игровом цикле:
         *
         *  time.tick();
         *  while (time.shouldUpdate(FIXED_TIME_STEP)) {
         *      // fixed-step логика (мир, физика, AI и т.д.)
         *  }
         *
         * Накопление времени ведётся по scaled dt (с учётом паузы и timeScale), и ограничено
         * сверху константой kMaxAccumulatedSeconds, дабы не было "догоняющего ада" при огромном dt.
         */
        [[nodiscard]] bool shouldUpdate(const sf::Time& fixedTimeStep) noexcept;

        /// @brief Сбросить накопленное "симуляционное" время без полного reset().
        ///
        /// Полезно при:
        ///  - загрузке сохранения (чтобы не было скачка времени);
        ///  - резких скачках во времени/состоянии мира;
        ///  - достижении лимита апдейтов за кадр (spiral of death prevention).
        void clearAccumulatedTime() noexcept {
            mTimeSinceLastUpdate = sf::Time::Zero;
        }

        // ----------------------- Метрики времени ------------------------------------------------

        /// @brief "Сырое" dt с прошлого кадра (без учёта паузы и timeScale).
        [[nodiscard]] sf::Time getRawDeltaTime() const noexcept {
            return mRawDeltaTime;
        }
        [[nodiscard]] float getRawDeltaTimeSeconds() const noexcept {
            return mRawDeltaTime.asSeconds();
        }

        /// @brief "Симуляционный" dt (с паузой и timeScale).
        [[nodiscard]] sf::Time getDeltaTime() const noexcept {
            return mDeltaTime;
        }
        [[nodiscard]] float getDeltaTimeSeconds() const noexcept {
            return mDeltaTime.asSeconds();
        }

        /// @brief Мгновенный FPS по raw dt.
        [[nodiscard]] float getFps() const noexcept {
            return mFps;
        }

        /// @brief Сглаженный FPS (экспоненциальное сглаживание).
        [[nodiscard]] float getSmoothedFps() const noexcept {
            return mSmoothedFps;
        }

        /// @brief Накопленное "симуляционное" время для fixed-step (для отладки).
        [[nodiscard]] sf::Time getAccumulatedTime() const noexcept {
            return mTimeSinceLastUpdate;
        }

        /// @brief Количество кадров с момента последнего reset().
        [[nodiscard]] std::uint64_t getFrameCount() const noexcept {
            return mFrameCount;
        }

        /// @brief Минимальный FPS за сессию (с момента reset()).
        ///
        /// Если после reset() ещё не было ни одного валидного измерения (tick() не вызывался),
        /// возвращает 0.f как "нет данных", а не как фактическую просадку FPS до нуля.
        [[nodiscard]] float getMinFps() const noexcept {
            return (mMinFps == std::numeric_limits<float>::max()) ? 0.f : mMinFps;
        }

        /// @brief Максимальный FPS за сессию (с момента reset()).
        [[nodiscard]] float getMaxFps() const noexcept {
            return mMaxFps;
        }

        // ------------------- Управление временем игры -------------------------------------------

        /// @brief Установить/снять паузу симуляции.
        void setPaused(bool paused) noexcept {
            mPaused = paused;
        }

        [[nodiscard]] bool isPaused() const noexcept {
            return mPaused;
        }

        /**
         * @brief Установить множитель времени (0 — стоп, 1 — нормальная скорость, >1 — ускорение).
         *
         * scale < 0.f интерпретируется как 0.f (игра останавливается).
         * Рекомендуемые диапазоны:
         *  - 0.0f .. 4.0f для обычного геймплея;
         *  - больше — для режимов отладки/реплеев/быстрой перемотки.
         *
         * При очень больших значениях (x10, x50, x100) количество фиксированных обновлений за кадр
         * может сильно вырасти. Это:
         *  - НЕ приведёт к физическому повреждению CPU, но может заметно увеличить нагрузку и
         *    время, проводимое в логике мира.
         *
         * В будущей крупной 4X:
         *  - дополнительные лимиты/адаптация частоты обновлений стоит реализовывать
         *    на уровне игрового цикла / геймплейного слоя, а не внутри TimeService.
         */
        void setTimeScale(float scale) noexcept {
            if (scale <= 0.f) {
                mTimeScale = 0.f;
            } else {
                mTimeScale = scale;
            }
        }

        [[nodiscard]] float getTimeScale() const noexcept {
            return mTimeScale;
        }

      private:
        // Ограничения для защиты от экстремальных dt (секунды)
        static constexpr float kMaxDeltaSeconds = 0.25f; // clamp raw dt до 250 мс

        sf::Clock mClock;                // секундомер кадра
        sf::Time mRawDeltaTime{};        // "сырое" dt между кадрами
        sf::Time mDeltaTime{};           // "симуляционный" dt (с учётом паузы и timeScale)
        sf::Time mTimeSinceLastUpdate{}; // накопитель для fixed-step

        bool mInitialized{false};        // чтобы избежать скачка на первом кадре
        bool mPaused{false};             // флаг паузы симуляции
        float mTimeScale{1.f};           // множитель времени (1.0 — нормальная скорость)

        float mFps{0.f};                 // мгновенный FPS (по raw dt)
        float mSmoothedFps{0.f};         // сглаженный FPS

        // Базовые метрики для профилирования
        std::uint64_t mFrameCount{0};
        float mMinFps{std::numeric_limits<float>::max()};
        float mMaxFps{0.f};

    };

} // namespace core::time