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

#include <SFML/Graphics.hpp>

#include "core/ecs/system.h"

// Предварительные объявления, чтобы избежать лишних инклюдов, ускорить пересборку.
// Нужно для core::time::TimeService* mTime{ nullptr };
namespace core {
    namespace time {
        class TimeService;
    }
} // namespace core

namespace core::ecs {

    class DebugOverlaySystem final : public ISystem {
      public:
        DebugOverlaySystem() = default;

        struct Style {
            sf::Vector2f position{10.f, 10.f};
            unsigned int characterSize{35};
            sf::Color color{255, 0, 0, 255};
        };

        // Связать с сервисом времени и шрифтом (полученным через ResourceManager)
        void bind(core::time::TimeService& timeService, const sf::Font& font);

        void setEnabled(bool enabled) noexcept {
            mEnabled = enabled;
        }
        [[nodiscard]] bool isEnabled() const noexcept {
            return mEnabled;
        }

        void update(World&, float) override;
        void render(World&, sf::RenderWindow& window) override;

        void applyStyle(const Style& s);

      private:
        core::time::TimeService* mTime{nullptr}; // не владеем
        std::optional<sf::Text> mFpsText;
        bool mEnabled{true};
    };

} // namespace core::ecs