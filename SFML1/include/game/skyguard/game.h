// ============================================================================
// File: game/skyguard/game.h
// Role: High-level application for SkyGuard (window, loop, ECS wiring)
// Notes:
//  - Lives in the game layer (game/skyguard), depends on core engine only
//  - Owns TimeService and ECS World, wires SkyGuard-specific systems
//  - Does NOT belong to core/, and core/ does not depend on this header
// ============================================================================
#pragma once

#include <SFML/Graphics.hpp>

#include "core/ecs/world.h"
#include "core/resources/resource_manager.h"
#include "core/time/time_service.h"

namespace core::ecs {
    class ScalingSystem;
    class LockSystem;
    class InputSystem;
    class DebugOverlaySystem;
} // namespace core::ecs

namespace game::skyguard {

    class Game {
      public:
        Game();
        void run();

      private:
        void processEvents();
        void update(const sf::Time& dt);
        void render();
        void initWorld(); // создаёт ECS-мир и инициализирует сущности SkyGuard

      private:
        sf::RenderWindow mWindow;
        core::resources::ResourceManager mResources;

        core::time::TimeService mTime;   // сервис времени (вне ECS)
        core::ecs::World mWorld;         // ECS-мир
        core::ecs::Entity mPlayerEntity; // сущность игрока

        // Системы, к которым нужен прямой доступ
        core::ecs::ScalingSystem* mScalingSystem{nullptr};      // кэшируем систему масштабирования
        core::ecs::LockSystem* mLockSystem{nullptr};            // кэшируем систему фиксации
        core::ecs::InputSystem* mInputSystem{nullptr};          // клавиатурный ввод
        core::ecs::DebugOverlaySystem* mDebugOverlay{nullptr};  // debug overlay (FPS и т.п.)
    };

} // namespace game::skyguard