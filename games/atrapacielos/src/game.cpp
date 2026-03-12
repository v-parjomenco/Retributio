#include "pch.h"

#include "game.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

#include <SFML/Window/Keyboard.hpp>

#include "core/config/engine_settings.h"
#include "core/config/loader/debug_overlay_loader.h"
#include "core/config/loader/engine_settings_loader.h"
#include "core/debug/debug_config.h"
#include "core/ecs/entity.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/systems/debug_overlay_system.h"
#include "core/ecs/systems/movement_system.h"
#include "core/ecs/systems/render_system.h"
#include "core/ecs/systems/spatial_index_system.h"
#include "core/log/log_macros.h"
#include "core/resources/registry/resource_registry.h"
#include "core/time/time_config.h"
#include "bootstrap/scene_bootstrap.h"
#include "config/config_paths.h"
#include "config/loader/app_config_loader.h"
#include "config/loader/config_loader.h"
#include "config/loader/spatial_v2_config_builder.h"
#include "config/loader/user_settings_loader.h"
#include "config/loader/view_config_loader.h"
#include "config/loader/window_config_loader.h"
#include "config/window_config.h"
#include "ecs/components/player_tag_component.h"
#include "ecs/systems/aircraft_control_system.h"
#include "ecs/systems/player_bounds_system.h"
#include "ecs/systems/player_init_system.h"
#include "ecs/systems/spatial_streaming_system.h"
#include "ecs/queries/local_player_query.h"
#include "platform/user_paths.h"
#include "presentation/view_manager.h"

#if defined(RETRIBUTIO_PROFILE)
    #include "dev/stress_render_chunk_content_provider.h"
    #include "dev/stress_chunk_content_provider.h"
    #include "dev/stress_runtime_stamp.h"
    #include "dev/stress_render_options.h"
#endif
#if !defined(NDEBUG) || defined(RETRIBUTIO_PROFILE)
    #include "dev/overlay_extras.h"
#endif

// Движковые конфиги (Vsync, frame limit и т.п.).
namespace cfg = ::core::config;
// Движковые настройки времени (фиксированный timestep и т.п.).
namespace timecfg = ::core::time;
// Debug-флаги и хоткеи (оверлей, удержание при выходе и т.п.).
namespace dbg = ::core::debug;
// Специфические игровые конфиги/blueprints для Atrapacielos (player.json, window и т.п.).
namespace skycfg = ::game::atrapacielos::config;
// Централизованное хранилище путей к JSON-конфигам.
namespace skycfg_paths = ::game::atrapacielos::config::paths;
// Платформенные утилиты Atrapacielos (OS-стандартные пути записи/чтения и т.п.).
namespace platform = ::game::atrapacielos::platform;

namespace game::atrapacielos {

    Game::Game() {

        // ----------------------------------------------------------------------------------------
        // Загружаем app identity (единый источник: app.id + app.display_name)
        // ----------------------------------------------------------------------------------------
        const skycfg::AppConfig appCfg = skycfg::loadAppConfig(skycfg_paths::ATRAPACIELOS_GAME);

        // ----------------------------------------------------------------------------------------
        // Загружаем конфиг окна и view Atrapacielos из JSON (atrapacielos.json)
        // ----------------------------------------------------------------------------------------
        const skycfg::WindowConfig windowCfg = 
            skycfg::loadWindowConfig(skycfg_paths::ATRAPACIELOS_GAME);
        const skycfg::ViewConfig viewCfg = 
            skycfg::loadViewConfig(skycfg_paths::ATRAPACIELOS_GAME);

        // ----------------------------------------------------------------------------------------
        // Загружаем пользовательские настройки (переопределяя дефолты)
        //  из стандартного пути записи ОС
        // ----------------------------------------------------------------------------------------
        mUserSettingsPath = platform::getUserSettingsPath(appCfg.id);
#if !defined(NDEBUG)
        try {
            LOG_DEBUG(core::log::cat::Config, "[UserSettingsLoader] Path: '{}'",
                      mUserSettingsPath.string());
        } catch (...) {
            LOG_DEBUG(core::log::cat::Config,
                      "[UserSettingsLoader] Path: <failed to stringify std::filesystem::path>");
        }
#endif
        mUserSettings = skycfg::loadUserSettings(mUserSettingsPath);

        const skycfg::WindowConfig effectiveWindowCfg =
            skycfg::applyUserSettings(windowCfg, mUserSettings);

        mWindowModeManager.init(effectiveWindowCfg, appCfg.displayName);
        if (!mWindowModeManager.createInitial(mWindow) || !mWindow.isOpen()) {
            throw std::runtime_error("Failed to create main window");
        }
        mWindow.setKeyRepeatEnabled(false);

        // ----------------------------------------------------------------------------------------
        // Загружаем движковые настройки рендеринга (EngineSettings)
        // ----------------------------------------------------------------------------------------
        mEngineSettings = cfg::loadEngineSettings(skycfg_paths::ENGINE_SETTINGS);
        applyEngineSettingsToWindow();

        LOG_INFO(core::log::cat::Gameplay, "[EngineSettings] VSync: {}, frameLimit: {}{}",
                 (mEngineSettings.vsyncEnabled ? "enabled" : "disabled"),
                 mEngineSettings.frameLimit,
                 (mEngineSettings.vsyncEnabled ? " (VSync enabled, frameLimit ignored)."
                                               : " (VSync disabled, frameLimit applied)."));

        // ----------------------------------------------------------------------------------------
        // Инициализация view (letterbox + UI separation)
        // ----------------------------------------------------------------------------------------
        mViewManager.init(viewCfg, mWindow.getSize());

        // ----------------------------------------------------------------------------------------
        // Инициализация ресурсного слоя (реестр ресурсов + fallback-ресурсы)
        // ----------------------------------------------------------------------------------------
        initResources();

        // ----------------------------------------------------------------------------------------
        // Создаём ECS-мир и игровые сущности Atrapacielos
        // ----------------------------------------------------------------------------------------
        initWorld();

        // После init/preload запрещаем любые lazy-load попытки (runtime contract).
        mResources.setIoForbidden(true);
    }

    void Game::applyEngineSettingsToWindow() noexcept {
        mWindow.setVerticalSyncEnabled(mEngineSettings.vsyncEnabled);
        if (!mEngineSettings.vsyncEnabled) {
            mWindow.setFramerateLimit(mEngineSettings.frameLimit);
        } else {
            mWindow.setFramerateLimit(0);
        }
    }

    void Game::initResources() {
        const std::array<core::resources::ResourceSource, 1> sources{
            core::resources::ResourceSource{
                std::string(skycfg_paths::RESOURCES), 0, 0, "atrapacielos"}
        };
        mResources.initialize(sources);
    }

    // initWorld() без try/catch — все исключения уходят наверх в main()
    void Game::initWorld() {
        auto playerCfg = skycfg::ConfigLoader::loadPlayerConfig(mResources, skycfg_paths::PLAYER);
        const float playerFloorY = mViewManager.getWorldLogicalSize().y;

        std::vector<game::atrapacielos::config::blueprints::PlayerBlueprint> players;
        players.emplace_back(std::move(playerCfg));

        // ----------------------------------------------------------------------------------------
        // Scene bootstrap: resolve keys + preload + derived sprite data (validate-on-write).
        // Game остаётся дирижёром: реализация подготовки сцены вынесена в bootstrap-модуль.
        // ----------------------------------------------------------------------------------------
        game::atrapacielos::bootstrap::SceneBootstrapConfig bootCfg{.players = std::span(players)};

        const game::atrapacielos::bootstrap::SceneBootstrapResult boot =
            game::atrapacielos::bootstrap::preloadAndResolveInitialScene(mResources, bootCfg);

        // ----------------------------------------------------------------------------------------
        // SpatialIndexSystem config (определяет expectedMaxEntities)
        // ----------------------------------------------------------------------------------------
        const core::ecs::SpatialIndexSystemConfig spatialCfg =
            config::buildSpatialIndexV2ConfigAtrapacielos(
                mEngineSettings, mViewManager.getWorldLogicalSize(), mWindow.getSize());

        // ----------------------------------------------------------------------------------------
        // Создание World с Prewarm
        // ----------------------------------------------------------------------------------------
        core::ecs::World::CreateInfo worldInfo{};
        worldInfo.reserveEntities = spatialCfg.maxEntityId;
        
        mWorld = std::make_unique<core::ecs::World>(worldInfo);

#if !defined(NDEBUG)
        if (spatialCfg.determinismEnabled) {
            mWorld->requireStableIdsForDeterminism();
        }
#endif

        if (spatialCfg.determinismEnabled) {
            auto& ids = mWorld->stableIds();
            ids.enable();
            const std::size_t capacity = config::computeStableIdCapacityAtrapacielos(spatialCfg);
            ids.prewarm(capacity);
            assert(ids.isEnabled() && ids.isPrewarmed() &&
                   "Game::initWorld: StableIdService wiring failed in deterministic mode");
        }

        // ----------------------------------------------------------------------------------------
        // Подключаем ECS-системы (порядок важен для update/render)
        // ----------------------------------------------------------------------------------------

        mWorld->addSystem<game::atrapacielos::ecs::PlayerInitSystem>(std::move(players));

        mAircraftControlSystem =
            &mWorld->addSystem<game::atrapacielos::ecs::AircraftControlSystem>();
        mWorld->addSystem<core::ecs::MovementSystem>();
        mWorld->addSystem<game::atrapacielos::ecs::PlayerBoundsSystem>(
            mViewManager.getWorldLogicalSize(), playerFloorY);

#if defined(RETRIBUTIO_PROFILE)
        if (!boot.stressPlayerKey.has_value()) {
            LOG_PANIC(core::log::cat::Config,
                      "Game::initWorld: stressPlayerKey must be resolved in PROFILE builds.");
        }

        const dev::StressMode stressMode = dev::readStressModeFromEnv();

        if (stressMode == dev::StressMode::Render) {
            const sf::Vector2f worldLogical = mViewManager.getWorldLogicalSize();

            auto stressProvider = std::make_unique<dev::StressRenderChunkContentProvider>(
                mResources, *boot.stressPlayerKey, spatialCfg.index.chunkSizeWorld,
                spatialCfg.storage.width, spatialCfg.storage.height,
                spatialCfg.hysteresisMarginChunks,
                worldLogical.x, worldLogical.y);

            mStressStamp = dev::buildRenderStressRuntimeStamp(
                *stressProvider, spatialCfg.storage.width, spatialCfg.storage.height);

            mChunkContentProvider = std::move(stressProvider);
        } else if (stressMode == dev::StressMode::Spatial) {
            auto stressProvider = std::make_unique<dev::StressChunkContentProvider>(
                mResources, *boot.stressPlayerKey, spatialCfg.index.chunkSizeWorld);

            mStressStamp = dev::buildSpatialStressRuntimeStamp(
                *stressProvider, spatialCfg.storage.width, spatialCfg.storage.height);

            mChunkContentProvider = std::move(stressProvider);
        } else {
            // StressMode::None in Profile: no stress content.
            mChunkContentProvider =
                std::make_unique<game::atrapacielos::streaming::EmptyChunkContentProvider>();
        }

#else
        // Debug/Release: no stress content (empty provider).
        mChunkContentProvider =
            std::make_unique<game::atrapacielos::streaming::EmptyChunkContentProvider>();
#endif

        auto& streamingSystem =
            mWorld->addSystem<game::atrapacielos::ecs::SpatialStreamingSystem>(spatialCfg);
        mSpatialStreamingSystem = &streamingSystem;

        auto& spatialSystem = mWorld->addSystem<core::ecs::SpatialIndexSystem>(spatialCfg);
        mSpatialIndexSystem = &spatialSystem;
        mFrameOrchestrator.bindSpatialIndexSystem(mSpatialIndexSystem);

        streamingSystem.bind(&spatialSystem, mChunkContentProvider.get());

        auto& renderSys = mWorld->addSystem<core::ecs::RenderSystem>();
        renderSys.bind(&spatialSystem.index(),
                       spatialSystem.entitiesBySpatialId(),
                       spatialCfg.maxVisibleSprites,
                       &mResources);
        mRenderSystem = &renderSys;

        mDebugOverlay = &mWorld->addSystem<core::ecs::DebugOverlaySystem>();

        {
            const sf::Font& font = mResources.expectFontResident(boot.defaultFontKey).get();

            mDebugOverlay->bind(mTime, font);
            mDebugOverlay->setRenderSystem(mRenderSystem);
            mDebugOverlay->setSpatialIndexSystem(mSpatialIndexSystem);

            const auto overlayCfg = cfg::loadDebugOverlayBlueprint(skycfg_paths::DEBUG_OVERLAY);

            mDebugOverlay->applyTextProperties(overlayCfg.text);
            mDebugOverlay->applyRuntimeProperties(overlayCfg.runtime);

            mDebugOverlay->setEnabled(overlayCfg.enabled && dbg::SHOW_FPS_OVERLAY);

#if defined(RETRIBUTIO_PROFILE)
            const bool backplateEnabled = dev::readRenderStressOverlayBackplateEnabled();
            mDebugOverlay->setBackgroundPanelEnabled(backplateEnabled);
            mDebugOverlay->setBackgroundPanelColor(sf::Color(0u, 0u, 0u, 102u));
            const sf::Vector2f uiSize = mViewManager.getUiLogicalSize();
            mDebugOverlay->setBackgroundPanelRect(
                sf::FloatRect{{0.f, 0.f}, {uiSize.x, uiSize.y * 0.46f}});
#endif
        }

        mBackgroundRenderer.init(mResources, boot.backgroundKey);
    }

    void Game::persistUserSettings() noexcept {
        if (mUserSettingsSavingDisabled) {
            return;
        }
        if (!skycfg::saveUserSettingsAtomic(mUserSettingsPath, mUserSettings)) {
            mUserSettingsSavingDisabled = true;
            LOG_WARN(core::log::cat::Config,
                     "[UserSettings] Не удалось сохранить настройки пользователя. "
                     "Дальнейшие попытки сохранения отключены на время сессии.");
        }
    }

    void Game::run() {
        assert(mWindow.isOpen());

        const sf::Time fixedTimeStep = timecfg::FIXED_TIME_STEP;

        const int maxUpdatesPerFrame = static_cast<int>(
            core::time::TimeService::kMaxAccumulatedSeconds / fixedTimeStep.asSeconds());

        LOG_INFO(core::log::cat::Gameplay, "Game loop started");

#if !defined(NDEBUG) || defined(RETRIBUTIO_PROFILE)
        std::uint64_t frameCount = 0;
#endif

        while (mWindow.isOpen()) {
            mTime.tick();
            processEvents();

            int updateCount = 0;
            while (mTime.shouldUpdate(fixedTimeStep)) {
                update(fixedTimeStep);
                if (++updateCount >= maxUpdatesPerFrame) {
                    mTime.clearAccumulatedTime();
                    break;
                }
            }

            render();

#if !defined(NDEBUG) || defined(RETRIBUTIO_PROFILE)
            if (++frameCount % 600ULL == 0ULL) {
                LOG_DEBUG(core::log::cat::Performance, "FPS: {:.1f} (frame {})",
                          mTime.getSmoothedFps(), frameCount);
            }
#endif
        }
    }

    void Game::processEvents() {
        assert(mAircraftControlSystem != nullptr);
        assert(mDebugOverlay != nullptr);

        for (;;) {
            const std::optional<sf::Event> eventOpt = mWindow.pollEvent();
            if (!eventOpt.has_value()) {
                break;
            }

            const sf::Event& event = *eventOpt;

            if (event.is<sf::Event::Closed>()) {
                mWindow.close();
            }
            else if (const auto* keyPressed = event.getIf<sf::Event::KeyPressed>()) {
                if (keyPressed->alt && keyPressed->code == sf::Keyboard::Key::Enter) {
                    mWindowModeManager.requestCycleMode();
                } else {
                    mAircraftControlSystem->onKeyEvent(keyPressed->code, true);

                    if (keyPressed->code == dbg::HOTKEY_TOGGLE_OVERLAY) {
                        mDebugOverlay->setEnabled(!mDebugOverlay->isEnabled());
                    }

#if !defined(NDEBUG)
                    if (keyPressed->code == dbg::HOTKEY_DUMP_CAMERA) {
                        core::ecs::Entity e{};
                        const core::ecs::TransformComponent* tr = nullptr;

                        if (!ecs::queries::tryGetLocalPlayerTransform(*mWorld, e, tr)) {
                            LOG_DEBUG(core::log::cat::Gameplay, "CamDebug: no local player");
                        } else {
                            const auto& vw = mViewManager.getWorldView();
                            const sf::Vector2f off = mViewManager.getCameraOffset();

                            LOG_DEBUG(core::log::cat::Gameplay,
                                      "CamDebug: playerY={:.2f} viewCenterY={:.2f} viewSizeY={:.2f}"
                                      " cameraOffsetY={:.2f} cameraCenterYMax={:.2f}",
                                      tr->position.y, vw.getCenter().y, vw.getSize().y, off.y,
                                      mViewManager.getCameraCenterYMax());
                        }
                    }
#endif
                }
            }
            else if (const auto* keyReleased = event.getIf<sf::Event::KeyReleased>()) {
                mAircraftControlSystem->onKeyEvent(keyReleased->code, false);
            }
            else if (event.is<sf::Event::FocusLost>()) {
                mAircraftControlSystem->resetState();
            }
            else if (const auto* resized = event.getIf<sf::Event::Resized>()) {
                const sf::Vector2u newSize{resized->size.x, resized->size.y};
                if (newSize.x == 0u || newSize.y == 0u) {
                    continue;
                }

                mViewManager.onResize(newSize);
                mWindowModeManager.onWindowResized(newSize);

                if (mWindowModeManager.getMode() == skycfg::WindowMode::Windowed) {
                    const bool changed =
                        skycfg::setWindowedSize(mUserSettings, newSize.x, newSize.y);
                    if (changed) {
                        persistUserSettings();
                    }
                }
            }
        }

        if (mWindow.isOpen() && mWindowModeManager.applyPending(mWindow)) {
            mWindow.setKeyRepeatEnabled(false);
            applyEngineSettingsToWindow();
            mViewManager.onResize(mWindow.getSize());

            if (mAircraftControlSystem) {
                mAircraftControlSystem->resetState();
            }

            bool changed = false;
            changed = skycfg::setWindowMode(mUserSettings, mWindowModeManager.getMode()) || changed;

            if (mWindowModeManager.getMode() == skycfg::WindowMode::Windowed) {
                const sf::Vector2u sz = mWindow.getSize();
                changed = skycfg::setWindowedSize(mUserSettings, sz.x, sz.y) || changed;
            }

            if (changed) {
                persistUserSettings();
            }
        }
    }

    void Game::update(const sf::Time& dt) {
        const float dtSeconds = dt.asSeconds();
        assert(dtSeconds > 0.0f);
        mFrameOrchestrator.beginFrameRead();
        mWorld->update(dtSeconds);
        updateCamera();
        mWorld->flushDestroyed();
    }

    void Game::updateCamera() {
        core::ecs::Entity e{};
        const core::ecs::TransformComponent* tr = nullptr;

        const bool foundPlayer = ecs::queries::tryGetLocalPlayerTransform(*mWorld, e, tr);
        if (foundPlayer) {
            mViewManager.updateCamera(tr->position);
        }

#if !defined(NDEBUG)
        static bool warnedOnce = false;
        if (!foundPlayer && !warnedOnce) {
            LOG_DEBUG(core::log::cat::Gameplay,
                      "Game::updateCamera: no LocalPlayerTagComponent found");
            warnedOnce = true;
        }
#endif
    }

    void Game::render() {
        mWindow.clear(sf::Color::Black);
        renderWorldPass();
        renderUiPass();
        mWindow.display();
    }

    void Game::renderWorldPass() {
        mWindow.setView(mViewManager.getWorldView());
        mBackgroundRenderer.update(mViewManager.getWorldView());
        mBackgroundRenderer.draw(mWindow);
        if (mRenderSystem) {
            mRenderSystem->render(*mWorld, mWindow);
        }
    }

    void Game::renderUiPass() {
        mWindow.setView(mViewManager.getUiView());
        if (!mDebugOverlay) {
            return;
        }

    #if !defined(NDEBUG) || defined(RETRIBUTIO_PROFILE)
        if (mDebugOverlay->isEnabled()) {
            dev::populateDebugOverlayExtraLines(
                *mDebugOverlay, *mWorld, mSpatialIndexSystem,
                mSpatialStreamingSystem,
                mBackgroundRenderer, mViewManager
    #if defined(RETRIBUTIO_PROFILE)
                , &mStressStamp
    #endif
            );
        }
    #endif

        mDebugOverlay->prepareFrame(*mWorld);
        mDebugOverlay->draw(mWindow);
    }

} // namespace game::atrapacielos