// ================================================================================================
// File: game/skyguard/game.h
// Role: High-level application for SkyGuard (window, loop, ECS wiring)
// Notes:
//  - Lives in the game layer (game/skyguard), depends on core engine only
//  - Owns TimeService and ECS World, wires SkyGuard-specific systems
//  - Implements a fixed-timestep game loop using core::time::TimeService
//  - Does NOT belong to core/, and core/ does not depend on this header
// ================================================================================================
#pragma once

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Time.hpp>

#include "core/ecs/world.h"
#include "core/resources/resource_manager.h"
#include "core/time/time_service.h"

namespace core::ecs {
    class ScalingSystem;
    class LockSystem;
    class DebugOverlaySystem;
    class RenderSystem;
} // namespace core::ecs

namespace game::skyguard::ecs {
    class AircraftControlSystem;
} // namespace game::skyguard::ecs

namespace game::skyguard {

    class Game {
      public:
        Game();

        Game(const Game&) = delete;
        Game& operator=(const Game&) = delete;

        Game(Game&&) = delete;
        Game& operator=(Game&&) = delete;

        void run();

      private:
        void processEvents();
        void update(const sf::Time& dt);
        void render();

        /// Инициализация ресурсного слоя (ResourcePaths + fallback-ресурсы ResourceManager).
        void initResources();

        /// Создаёт ECS-мир и инициализирует сущности SkyGuard.
        void initWorld();

      private:
        sf::RenderWindow mWindow;
        core::resources::ResourceManager mResources;

        core::time::TimeService mTime; // сервис времени (вне ECS)
        core::ecs::World mWorld;       // ECS-мир

        // Пока не используется:
        //  после перевода на one-shot PlayerInitSystem игрок создаётся внутри системы.
        // Если понадобится доступ к player entity (камера/фокус/мультиплеер),
        //  добавим явный канал:
        //  - либо возвращаем список созданных Entity из PlayerInitSystem,
        //  - либо заводим отдельный runtime-компонент "PlayerTag"/"LocalPlayerIndex"
        //  и ищем через view().
        core::ecs::Entity mPlayerEntity{core::ecs::NullEntity};

        // Системы, к которым нужен прямой доступ.
        //
        // ВАЖНО (про валидность адреса):
        //  - Сейчас SystemManager хранит системы через std::unique_ptr -> объекты живут в куче.
        //  - Реаллокации std::vector двигают unique_ptr, но НЕ двигают сами объекты систем.
        //  - Поэтому эти raw pointers стабильны по адресу, пока живёт World/SystemManager.
        core::ecs::ScalingSystem* mScalingSystem{nullptr};
        core::ecs::LockSystem* mLockSystem{nullptr};
        game::skyguard::ecs::AircraftControlSystem* mAircraftControlSystem{nullptr};
        core::ecs::DebugOverlaySystem* mDebugOverlay{nullptr};
        core::ecs::RenderSystem* mRenderSystem{nullptr};
    };

} // namespace game::skyguard
