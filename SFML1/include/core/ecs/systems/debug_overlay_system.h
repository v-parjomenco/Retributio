// ================================================================================================
// File: core/ecs/systems/debug_overlay_system.h
// Role: Lightweight ECS system to render debug overlay (FPS etc.)
// Notes:
//  - Reads data from services (e.g., TimeService), renders via SFML
//  - Data-driven friendly: font is provided by caller (typically resolved via ResourceManager);
//                          runtime layout comes from DebugOverlayBlueprint (JSON)
//  - Can be toggled on/off at runtime
// ================================================================================================

// TODO:
//
// дополнительные метрики : dt, fixed tick, позиции сущности игрока;
//
// позже: выделить Hud / UI Manager(управление всем UI, включая отладочный слой)
//  и оставить DebugOverlay как подсистему или виджет.

#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/System/Time.hpp>

#include "core/ecs/system.h"

// Предварительные объявления, чтобы избежать лишних инклюдов и ускорить пересборку.
namespace core {
    namespace time {
        class TimeService;
    } // namespace time
    namespace config {
        namespace properties {
            struct TextProperties;
            struct DebugOverlayRuntimeProperties;
        }
    } // namespace config
} // namespace core

namespace core::ecs {

    class RenderSystem;

    class DebugOverlaySystem final : public ISystem {
      public:
        DebugOverlaySystem() = default;

        // Связать с сервисом времени и шрифтом (полученным через ResourceManager).
        void bind(core::time::TimeService& timeService, const sf::Font& font);

        /**
         * @brief Привязать RenderSystem для отображения статистики рендера.
         *
         * УСЛОВНАЯ КОМПИЛЯЦИЯ:
         *  - Debug/Profile: сохраняет указатель, render() показывает статистику
         *  - Release: no-op (статистика не собирается, показывать нечего)
         *
         * Метод существует во всех сборках для совместимости вызывающего кода.
         */
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        void setRenderSystem(const RenderSystem* renderSystem) noexcept {
            mRenderSystem = renderSystem;
        }
#else
        void setRenderSystem(const RenderSystem*) noexcept {
            // Release: без действия. Статистика RenderSystem не собирается, показывать нечего.
        }
#endif

        void setEnabled(bool enabled) noexcept {
            mEnabled = enabled;
        }
        [[nodiscard]] bool isEnabled() const noexcept {
            return mEnabled;
        }

        void update(World&, float) override;
        /**
         * @brief Подготовить текст оверлея для текущего кадра.
         *
         * Вызывается один раз за кадр из игрового слоя.
         * Не рисует — только обновляет внутренний sf::Text.
         */
        void prepareFrame(World& world);
        void prepareFrame(World& world, std::string_view extraText);

        void clearExtraText() noexcept;
        void appendExtraLine(std::string_view line);

        /**
         * @brief Нарисовать оверлей.
         *
         * Вызывается в UI-проходе, когда уже выставлен нужный view.
         */
        void draw(sf::RenderWindow& window) const;

        /**
         * @brief Применить визуальные свойства текста (позиция, размер, цвет).
         *
         * Система ничего не знает о JSON/DebugOverlayBlueprint.
         * Она получает уже готовый набор свойств текста и лишь
         * отображает их на sf::Text.
         */
        void applyTextProperties(const core::config::properties::TextProperties& props);
        void applyRuntimeProperties(
            const core::config::properties::DebugOverlayRuntimeProperties& props) noexcept;

      private:
        core::time::TimeService* mTime{nullptr}; // не владеем
        std::optional<sf::Text> mFpsText;
        std::string mTextBuffer; // scratch buffer (без аллокаций в каждом кадре)
        std::string mExtraTextBuffer;
        bool mEnabled{true};

        // Обновляем строку не каждый кадр, чтобы не было дрожи и лишней работы.
        // Throttling — сознательная фича: снижает стоимость форматирования/EMA,
        // когда оверлей включён, но точность "каждый кадр" не нужна.
        // Управляется через DebugOverlayRuntimeProperties::updateIntervalMs (JSON).
        // 0ms => обновлять каждый кадр.
        sf::Clock mRenderClock;
        sf::Time  mAccumulatedTime{};
        sf::Time  mUpdateInterval{}; // 0 => каждый кадр (без троттлинга)
        std::uint8_t mSmoothingShift = 3;

        // ----------------------------------------------------------------------------------------
        // Только для Debug/Profile: статистика рендера из RenderSystem
        // ----------------------------------------------------------------------------------------
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        const RenderSystem* mRenderSystem{nullptr}; // не владеем
#endif

#if defined(SFML1_PROFILE)
        // Сглаженные значения времени (EMA), чтобы цифры не "прыгали".
        std::uint64_t mSmoothedCpuTotalUs = 0;
        std::uint64_t mSmoothedCpuDrawUs  = 0;

        // Сглаженная разбивка RenderSystem (raw/sm), только для Profile.
        std::uint64_t mSmoothedRSGatherUs = 0;
        std::uint64_t mSmoothedRSSortUs   = 0;
        std::uint64_t mSmoothedRSBuildUs  = 0;
        std::uint64_t mSmoothedRSDrawUs   = 0;
#endif

    };

} // namespace core::ecs