// ================================================================================================
// File: core/ecs/systems/debug_overlay_system.h
// Role: Lightweight ECS system to render debug overlay (FPS etc.)
// Notes:
//  - Reads data from services (e.g., TimeService), renders via SFML
//  - Data-driven friendly: font comes from ResourceManager; later add JSON config
//  - Can be toggled on/off at runtime
// ================================================================================================

// TODO:
//
// режимы сборки : #ifdef _DEBUG можно использовать для дефолта SHOW_FPS_OVERLAY;
// дополнительные метрики : ms / frame, dt, fixed tick, count draw calls(если есть),
//  позиции сущности игрока;
//
// конфигурирование(JSON) : добавить ещё пару метрик (ms/frame, dt, позиция игрока)
//  и аккуратно свести их в один текстовый блок с выравниванием;
//
// позже: выделить Hud / UI Manager(управление всем UI, включая отладочный слой)
//  и оставить DebugOverlay как подсистему или виджет.

#pragma once
#include <optional>
#include <string>

#include <SFML/Graphics.hpp>

#include "core/ecs/system.h"

// Предварительные объявления, чтобы избежать лишних инклюдов и ускорить пересборку.
namespace core {
    namespace time {
        class TimeService;
    } // namespace time
    namespace config {
        namespace properties {
            struct TextProperties;
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

      private:
        core::time::TimeService* mTime{nullptr}; // не владеем
        const RenderSystem* mRenderSystem{nullptr}; // не владеем
        std::optional<sf::Text> mFpsText;
        std::string mTextBuffer; // scratch buffer (no per-frame allocations)
        bool mEnabled{true};
    };

} // namespace core::ecs