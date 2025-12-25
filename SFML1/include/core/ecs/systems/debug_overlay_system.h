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
// дополнительные метрики : ms / frame, dt, fixed tick, count draw calls(если есть),
//  позиции сущности игрока;
//
// позже: выделить Hud / UI Manager(управление всем UI, включая отладочный слой)
//  и оставить DebugOverlay как подсистему или виджет.

#pragma once
#include <cstdint>
#include <optional>
#include <string>

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

        // Связать с сервисом времени и шрифтом (полученным через ResourceManager)
        void bind(core::time::TimeService& timeService, const sf::Font& font);
        void setRenderSystem(const RenderSystem* renderSystem) noexcept {
            mRenderSystem = renderSystem;
        }

        void setEnabled(bool enabled) noexcept {
            mEnabled = enabled;
        }
        [[nodiscard]] bool isEnabled() const noexcept {
            return mEnabled;
        }

        void update(World&, float) override;
        void render(World&, sf::RenderWindow& window) override;

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
        const RenderSystem* mRenderSystem{nullptr}; // не владеем
        std::optional<sf::Text> mFpsText;
        std::string mTextBuffer; // scratch buffer (no per-frame allocations)
        bool mEnabled{true};

        // Обновляем строку не каждый кадр, чтобы не было дрожи и лишней работы.
        sf::Clock mRenderClock;
        sf::Time  mAccumulatedTime{};
        sf::Time  mUpdateInterval{}; // 0 => каждый кадр (no throttle)
        std::uint8_t mSmoothingShift = 3;
#if defined(SFML1_PROFILE)
        // Сглаженные значения времени (EMA), чтобы цифры не "прыгали".
        std::uint64_t mSmoothedCpuTotalUs = 0;
        std::uint64_t mSmoothedCpuDrawUs  = 0;

        // Сглаженный breakdown RenderSystem (raw/sm), только для Profile.
        std::uint64_t mSmoothedRSGatherUs = 0;
        std::uint64_t mSmoothedRSSortUs   = 0;
        std::uint64_t mSmoothedRSBuildUs  = 0;
        std::uint64_t mSmoothedRSDrawUs   = 0;
#endif

    };

} // namespace core::ecs