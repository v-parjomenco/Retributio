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

#include <memory>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Time.hpp>

#include "core/config/engine_settings.h"
#include "core/ecs/world.h"
#include "core/resources/resource_manager.h"
#include "core/time/time_service.h"
#include "game/skyguard/config/loader/user_settings_loader.h"
#include "game/skyguard/presentation/background_renderer.h"
#include "game/skyguard/presentation/view_manager.h"
#include "game/skyguard/presentation/window_mode_manager.h"
#include "game/skyguard/streaming/chunk_content_provider.h"

namespace core::ecs {
    class DebugOverlaySystem;
    class RenderSystem;
    class SpatialIndexSystem;
} // namespace core::ecs

namespace game::skyguard::ecs {
    class AircraftControlSystem;
    class SpatialStreamingSystem;
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
        void updateCamera();
        void renderWorldPass();
        void renderUiPass();

        void applyEngineSettingsToWindow() noexcept;

        /// Инициализация ресурсного слоя (ResourceRegistry + fallback-ресурсы ResourceManager).
        void initResources();

        /// Создаёт ECS-мир и инициализирует сущности SkyGuard.
        void initWorld();

        void persistUserSettings() noexcept;

      private:
        sf::RenderWindow mWindow;
        presentation::WindowModeManager mWindowModeManager;

        core::resources::ResourceManager mResources;
        core::config::EngineSettings mEngineSettings;

        core::time::TimeService mTime; // сервис времени (вне ECS)
        std::unique_ptr<core::ecs::World> mWorld;
        presentation::ViewManager mViewManager;
        presentation::BackgroundRenderer mBackgroundRenderer;

        // После перевода на одноразовый PlayerInitSystem игрок создаётся внутри системы,
        //  а доступ к локальному игроку уже обеспечен через LocalPlayerTagComponent.
        // Если понадобится прямой доступ к сущностям игроков (камера/фокус/мультиплеер),
        //  добавим явный канал:
        //  - либо возвращаем список созданных Entity из PlayerInitSystem,
        //  - либо используем PlayerTagComponent/LocalPlayerTagComponent и ищем через view()
        core::ecs::Entity mPlayerEntity{core::ecs::NullEntity};

        // Системы, к которым нужен прямой доступ.
        //
        // ВАЖНО (про валидность адреса):
        //  - Сейчас SystemManager хранит системы через std::unique_ptr -> объекты живут в куче.
        //  - Реаллокации std::vector двигают unique_ptr, но НЕ двигают сами объекты систем.
        //  - Поэтому эти raw pointers стабильны по адресу, пока живёт World/SystemManager.
        game::skyguard::ecs::AircraftControlSystem* mAircraftControlSystem{nullptr};
        game::skyguard::ecs::SpatialStreamingSystem* mSpatialStreamingSystem{nullptr};
        core::ecs::DebugOverlaySystem* mDebugOverlay{nullptr};
        core::ecs::RenderSystem* mRenderSystem{nullptr};
        core::ecs::SpatialIndexSystem* mSpatialIndexSystem{nullptr};
        std::unique_ptr<streaming::IChunkContentProvider> mChunkContentProvider{};

        game::skyguard::config::UserSettings mUserSettings{};
        std::filesystem::path mUserSettingsPath{};
        bool mUserSettingsSavingDisabled = false;
    };

} // namespace game::skyguard
