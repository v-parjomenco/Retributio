#include "pch.h"

#include "game/skyguard/game.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
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
#include "core/ecs/components/sprite_component.h"
#include "core/ecs/components/spatial_dirty_tag.h"
#include "core/ecs/components/spatial_id_component.h"
#include "core/ecs/components/spatial_streamed_out_tag.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/render/sprite_bounds.h"
#include "core/ecs/systems/debug_overlay_system.h"
#include "core/ecs/systems/movement_system.h"
#include "core/ecs/systems/render_system.h"
#include "core/ecs/systems/spatial_index_system.h"
#include "core/log/log_macros.h"
#include "core/resources/registry/resource_registry.h"
#include "core/spatial/aabb2.h"
#include "core/spatial/chunk_coord.h"
#include "core/time/time_config.h"
#include "core/utils/math_constants.h"
#include "game/skyguard/bootstrap/scene_bootstrap.h"
#include "game/skyguard/config/config_paths.h"
#include "game/skyguard/config/loader/app_config_loader.h"
#include "game/skyguard/config/loader/config_loader.h"
#include "game/skyguard/config/loader/spatial_v2_config_builder.h"
#include "game/skyguard/config/loader/user_settings_loader.h"
#include "game/skyguard/config/loader/view_config_loader.h"
#include "game/skyguard/config/loader/window_config_loader.h"
#include "game/skyguard/config/window_config.h"
#include "game/skyguard/ecs/components/player_tag_component.h"
#include "game/skyguard/ecs/systems/aircraft_control_system.h"
#include "game/skyguard/ecs/systems/player_bounds_system.h"
#include "game/skyguard/ecs/systems/player_init_system.h"
#include "game/skyguard/ecs/systems/spatial_streaming_system.h"
#include "game/skyguard/platform/user_paths.h"
#include "game/skyguard/presentation/view_manager.h"
#include "game/skyguard/utils/debug_format.h"

#if defined(SFML1_PROFILE)
    #include "game/skyguard/dev/stress_chunk_content_provider.h"
#endif
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
    #include "game/skyguard/dev/spatial_index_harness.h"
#endif

// Движковые конфиги (Vsync, frame limit и т.п.).
namespace cfg = ::core::config;
// Движковые настройки времени (фиксированный timestep и т.п.).
namespace timecfg = ::core::time;
// Debug-флаги и хоткеи (оверлей, удержание при выходе и т.п.).
namespace dbg = ::core::debug;
// Специфические игровые конфиги/blueprints для SkyGuard (player.json, window и т.п.).
namespace skycfg = ::game::skyguard::config;
// Централизованное хранилище путей к JSON-конфигам.
namespace skycfg_paths = ::game::skyguard::config::paths;
// Платформенные утилиты SkyGuard (OS-стандартные пути записи/чтения и т.п.).
namespace platform = ::game::skyguard::platform;

namespace {

    // O(1) поиск локального игрока: берём первую сущность из view и сразу возвращаем Transform.
    // Без сравнений с NullEntity → не зависит от нюансов EnTT null-типа и не плодит operator!=.
    [[nodiscard]] bool tryGetLocalPlayerTransform(
        core::ecs::World& world,
        core::ecs::Entity& outEntity,
        const core::ecs::TransformComponent*& outTransform) noexcept {

        auto view = world.view<game::skyguard::ecs::LocalPlayerTagComponent,
                               core::ecs::TransformComponent>();

        const auto it = view.begin();
        if (it == view.end()) {
            outEntity = core::ecs::NullEntity;
            outTransform = nullptr;
            return false;
        }

        const core::ecs::Entity e = *it;
        outEntity = e;
        outTransform = &view.get<core::ecs::TransformComponent>(e);
        return true;
    }

} // namespace

namespace game::skyguard {

    Game::Game() {

        // ----------------------------------------------------------------------------------------
        // Загружаем app identity (единый источник: app.id + app.display_name)
        // ----------------------------------------------------------------------------------------
        const skycfg::AppConfig appCfg = skycfg::loadAppConfig(skycfg_paths::SKYGUARD_GAME);

        // ----------------------------------------------------------------------------------------
        // Загружаем конфиг окна и view SkyGuard из JSON (skyguard_game.json)
        // ----------------------------------------------------------------------------------------
        const skycfg::WindowConfig windowCfg = 
            skycfg::loadWindowConfig(skycfg_paths::SKYGUARD_GAME);
        const skycfg::ViewConfig viewCfg = 
            skycfg::loadViewConfig(skycfg_paths::SKYGUARD_GAME);

        // ----------------------------------------------------------------------------------------
        // Загружаем пользовательские настройки (переопределяя дефолты)
        //  из стандартного пути записи ОС
        // ----------------------------------------------------------------------------------------
        mUserSettingsPath = platform::getUserSettingsPath(appCfg.id);
#if !defined(NDEBUG)
        // Диагностика пути user settings (только Debug, один раз на запуск).
        // Полезно для тестов на Linux/macOS, чтобы сразу видеть OS-standard location.
        try {
            LOG_DEBUG(core::log::cat::Config, "[UserSettingsLoader] Path: '{}'",
                      mUserSettingsPath.string());
        } catch (...) {
            // На Windows строковая конверсия path может теоретически бросить
            // (экзотика с кодировками).
            LOG_DEBUG(core::log::cat::Config,
                      "[UserSettingsLoader] Path: <failed to stringify std::filesystem::path>");
        }
#endif
        mUserSettings = skycfg::loadUserSettings(mUserSettingsPath);

        // Итоговый runtime window config: shipped -> user override.
        const skycfg::WindowConfig effectiveWindowCfg =
            skycfg::applyUserSettings(windowCfg, mUserSettings);

        // Создаём окно (режимы: windowed/borderless/fullscreen).
        // Используется менеджер для поддержки переключения режимов.
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

        // Логируем итоговые настройки рендеринга один раз при запуске игры.
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
        // Важно: initResources() должен быть вызван до initWorld(), чтобы:
        //  - реестр ресурсов был загружен;
        //  - fallback-ресурсы были настроены;
        //  - системы/лоадеры могли безопасно резолвить ресурсы через ResourceManager.
        initResources();

        // ----------------------------------------------------------------------------------------
        // Создаём ECS-мир и игровые сущности SkyGuard
        // ----------------------------------------------------------------------------------------
        initWorld();

        // После init/preload запрещаем любые lazy-load попытки (runtime contract).
        mResources.setIoForbidden(true);
    }

    void Game::applyEngineSettingsToWindow() noexcept {
        // Политика применения:
        //  - VSync ON  => frameLimit выключен (0), избегаем двойного ограничения.
        //  - VSync OFF => применяем frameLimit из конфига.
        mWindow.setVerticalSyncEnabled(mEngineSettings.vsyncEnabled);
        if (!mEngineSettings.vsyncEnabled) {
            mWindow.setFramerateLimit(mEngineSettings.frameLimit);
        } else {
            mWindow.setFramerateLimit(0);
        }
    }

    void Game::initResources() {
        // Инициализация key-world реестра ресурсов (v1).
        // Критичный конфиг: если он сломан — нет смысла продолжать игру.
        const std::array<core::resources::ResourceSource, 1> sources{
            core::resources::ResourceSource{std::string(skycfg_paths::RESOURCES), 0, 0, "skyguard"}
        };

        mResources.initialize(sources);

        // Fallback-ключи (core.texture.missing, core.font.default) валидируются
        // автоматически в ResourceManager::initialize().
    }

    // initWorld() без try/catch — все исключения уходят наверх в main()
    void Game::initWorld() {
        // Загружаем blueprint игрока (data-driven).
        auto playerCfg = skycfg::ConfigLoader::loadPlayerConfig(mResources, skycfg_paths::PLAYER);
        const float playerFloorY = mViewManager.getWorldLogicalSize().y;

        std::vector<game::skyguard::config::blueprints::PlayerBlueprint> players;
        players.emplace_back(std::move(playerCfg));

        // ----------------------------------------------------------------------------------------
        // Scene bootstrap: resolve keys + preload + derived sprite data (validate-on-write).
        // Game остаётся дирижёром: реализация подготовки сцены вынесена в bootstrap-модуль.
        // ----------------------------------------------------------------------------------------
        game::skyguard::bootstrap::SceneBootstrapConfig bootCfg{.players = std::span(players)};

        const game::skyguard::bootstrap::SceneBootstrapResult boot =
            game::skyguard::bootstrap::preloadAndResolveInitialScene(mResources, bootCfg);

        // ----------------------------------------------------------------------------------------
        // SpatialIndexSystem config (определяет expectedMaxEntities)
        // ----------------------------------------------------------------------------------------
        const core::ecs::SpatialIndexSystemConfig spatialCfg =
            config::buildSpatialIndexV2ConfigSkyGuard(
                mEngineSettings, mViewManager.getWorldLogicalSize(), mWindow.getSize());

        // ----------------------------------------------------------------------------------------
        // Создание World с Prewarm
        // ----------------------------------------------------------------------------------------
        core::ecs::World::CreateInfo worldInfo{};
        worldInfo.reserveEntities = spatialCfg.maxEntityId;
        
        mWorld = std::make_unique<core::ecs::World>(worldInfo);
        // ----------------------------------------------------------------------------------------

#if !defined(NDEBUG)
        if (spatialCfg.determinismEnabled) {
            mWorld->requireStableIdsForDeterminism();
        }
#endif

        // Deterministic wiring — включаем StableID сервис один раз до первого createEntity().
        if (spatialCfg.determinismEnabled) {
            auto& ids = mWorld->stableIds();
            ids.enable();

            const std::size_t capacity = config::computeStableIdCapacitySkyGuard(spatialCfg);
            ids.prewarm(capacity);

            assert(ids.isEnabled() && ids.isPrewarmed() &&
                   "Game::initWorld: StableIdService wiring failed in deterministic mode");
        }

        // ----------------------------------------------------------------------------------------
        // Подключаем ECS-системы (порядок важен для update/render)
        // ----------------------------------------------------------------------------------------

        // PlayerInitSystem сам создаёт сущности на первом тике и больше не работает.
        // ВАЖНО: система строго data-only (без ResourceManager): все derived поля уже resolved.
        mWorld->addSystem<game::skyguard::ecs::PlayerInitSystem>(std::move(players));

        // ВАЖНО: AircraftControl должен обновляться ДО Movement, т.к. пишет VelocityComponent,
        // а Movement читает VelocityComponent в этом же тике.
        mAircraftControlSystem = &mWorld->addSystem<game::skyguard::ecs::AircraftControlSystem>();
        mWorld->addSystem<core::ecs::MovementSystem>();
        mWorld->addSystem<game::skyguard::ecs::PlayerBoundsSystem>(
            mViewManager.getWorldLogicalSize(), playerFloorY);

#if defined(SFML1_PROFILE)
        // Profile: streaming stress provider (deterministic per-chunk).
        if (!boot.stressPlayerKey.has_value()) {
            LOG_PANIC(core::log::cat::Config,
                      "Game::initWorld: stressPlayerKey must be resolved in PROFILE builds.");
        }

        mChunkContentProvider = std::make_unique<game::skyguard::dev::StressChunkContentProvider>(
            mResources, *boot.stressPlayerKey, spatialCfg.index.chunkSizeWorld);
#else
        // Debug/Release: no stress content (empty provider).
        mChunkContentProvider =
            std::make_unique<game::skyguard::streaming::EmptyChunkContentProvider>();
#endif

        auto& streamingSystem =
            mWorld->addSystem<game::skyguard::ecs::SpatialStreamingSystem>(spatialCfg);
        mSpatialStreamingSystem = &streamingSystem;

        auto& spatialSystem = mWorld->addSystem<core::ecs::SpatialIndexSystem>(spatialCfg);
        mSpatialIndexSystem = &spatialSystem;

        streamingSystem.bind(&spatialSystem, mChunkContentProvider.get());

        // 1) Создаём систему рендеринга конструктором по умолчанию (без аргументов).
        auto& renderSys = mWorld->addSystem<core::ecs::RenderSystem>();
        // 2) Привязываем зависимости через bind().
        renderSys.bind(&spatialSystem.index(),
                       spatialSystem.entitiesBySpatialId(),
                       spatialCfg.maxVisibleSprites,
                       &mResources);
        // 3) Сохраняем указатель.
        mRenderSystem = &renderSys;

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        game::skyguard::dev::tryRunSpatialIndexHarnessFromEnv(spatialCfg);
#endif

        // Эти системы требуют прямого доступа (onResize, onKeyEvent),
        // поэтому сохраняем указатели.
        mDebugOverlay = &mWorld->addSystem<core::ecs::DebugOverlaySystem>();

        // Привязываем оверлей к сервису времени и шрифту (resident-only).
        // Важно: ресурсы резолвим один раз при старте, не в hot-path.
        {
            const sf::Font& font = mResources.expectFontResident(boot.defaultFontKey).get();

            mDebugOverlay->bind(mTime, font);
            mDebugOverlay->setRenderSystem(mRenderSystem);
            mDebugOverlay->setSpatialIndexSystem(mSpatialIndexSystem);

            // Грузим конфиг для DebugOverlay (dev/косметика, не должен валить игру).
            const auto overlayCfg = cfg::loadDebugOverlayBlueprint(skycfg_paths::DEBUG_OVERLAY);

            // Применяем стиль.
            mDebugOverlay->applyTextProperties(overlayCfg.text);
            mDebugOverlay->applyRuntimeProperties(overlayCfg.runtime);

            // Политика дефолтного состояния оверлея при старте:
            //  - overlayCfg.enabled может ОТКЛЮЧИТЬ оверлей по умолчанию;
            //  - dbg::SHOW_FPS_OVERLAY — compile-time флаг (Debug/Profile: true, Release: false).
            //
            // ВАЖНО: при формуле ниже JSON не может "включить оверлей в Release на старте",
            // потому что compile-time флаг сильнее. Но хоткей F3 (dbg::HOTKEY_TOGGLE_OVERLAY)
            // всё равно позволяет включить оверлей в рантайме в любой сборке.
            mDebugOverlay->setEnabled(overlayCfg.enabled && dbg::SHOW_FPS_OVERLAY);
        }

        // BackgroundRenderer: resident-only + generation-safe cache.
        mBackgroundRenderer.init(mResources, boot.backgroundKey);
    }

    void Game::persistUserSettings() noexcept {
        if (mUserSettingsSavingDisabled) {
            return;
        }

        if (!skycfg::saveUserSettingsAtomic(mUserSettingsPath, mUserSettings)) {
            mUserSettingsSavingDisabled = true;
            // Один WARN на сессию: не спамим.
            LOG_WARN(core::log::cat::Config,
                     "[UserSettings] Не удалось сохранить настройки пользователя. "
                     "Дальнейшие попытки сохранения отключены на время сессии.");
        }
    }

    void Game::run() {
        assert(mWindow.isOpen());

        const sf::Time fixedTimeStep = timecfg::FIXED_TIME_STEP;

        // ----------------------------------------------------------------------------------------
        // Spiral of death prevention: вычисляем max апдейтов за кадр из инварианта TimeService.
        // Формула выводится из константы TimeService::kMaxAccumulatedSeconds и fixed timestep.
        // При значениях (0.5s / (1/60)) ≈ 30 апдейтов максимум.
        // ----------------------------------------------------------------------------------------
        const int maxUpdatesPerFrame = static_cast<int>(
            core::time::TimeService::kMaxAccumulatedSeconds / fixedTimeStep.asSeconds());

        LOG_INFO(core::log::cat::Gameplay, "Game loop started");

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        std::uint64_t frameCount = 0;
#endif

        while (mWindow.isOpen()) {
            // Обновляем время кадра (raw dt, scaled dt, FPS/метрики).
            mTime.tick();

            // Обрабатываем события окна и ввода.
            processEvents();

            // ------------------------------------------------------------------------------------
            // Выполняем один или несколько фиксированных шагов логики.
            // Ограничение на количество апдейтов за кадр предотвращает спираль смерти при лагах.
            // ------------------------------------------------------------------------------------
            int updateCount = 0;
            while (mTime.shouldUpdate(fixedTimeStep)) {
                update(fixedTimeStep);

                if (++updateCount >= maxUpdatesPerFrame) {
                    // Достигнут лимит апдейтов за кадр → отбрасываем оставшийся "долг".
                    // Без этого долг переходит на следующий кадр (lag echo / mini spiral).
                    mTime.clearAccumulatedTime();
                    break;
                }
            }

            // Отрисовываем текущее состояние мира.
            render();

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
            // Ограничение частоты вывода отладочной информации, чтобы избежать спама логом.
            if (++frameCount % 600ULL == 0ULL) {
                LOG_DEBUG(core::log::cat::Performance, "FPS: {:.1f} (frame {})",
                          mTime.getSmoothedFps(), frameCount);
            }
#endif
        }
    }

    void Game::processEvents() {
        // Инварианты инициализации: Game::run() вызывается после ctor/initWorld().
        assert(mAircraftControlSystem != nullptr);
        assert(mDebugOverlay != nullptr);

        for (;;) {
            const std::optional<sf::Event> eventOpt = mWindow.pollEvent();
            if (!eventOpt.has_value()) {
                break;
            }

            const sf::Event& event = *eventOpt;

            // Закрытие окна.
            if (event.is<sf::Event::Closed>()) {
                mWindow.close();
            }
            // Нажатие клавиш.
            else if (const auto* keyPressed = event.getIf<sf::Event::KeyPressed>()) {

                // Alt+Enter: циклическое переключение режимов:
                //   Windowed -> BorderlessFullscreen -> Fullscreen -> Windowed.
                //
                // TODO (когда появится меню настроек):
                //  - Alt+Enter оставить как Windowed <-> Borderless
                //    (или Windowed <-> LastFullscreenMode),
                //  - Exclusive Fullscreen включать только через меню.
                if (keyPressed->alt && keyPressed->code == sf::Keyboard::Key::Enter) {
                    // ВАЖНО: не используем continue после вызова requestCycleMode().
                    // Если в сигнатуре requestCycleMode() случайно окажется [[noreturn]],
                    // MSVC будет ругаться на "недостижимый код" сразу на continue/следующей строке.
                    mWindowModeManager.requestCycleMode();
                } else {
                    mAircraftControlSystem->onKeyEvent(keyPressed->code, true);

                    // Debug overlay toggle: хоткей работает во всех сборках.
                    // Политика: хоткей ВСЕГДА активен, независимо от дефолтного состояния overlay
                    // при старте.
                    if (keyPressed->code == dbg::HOTKEY_TOGGLE_OVERLAY) {
                        mDebugOverlay->setEnabled(!mDebugOverlay->isEnabled());
                    }

#if !defined(NDEBUG)
                    if (keyPressed->code == dbg::HOTKEY_DUMP_CAMERA) {
                        core::ecs::Entity e{};
                        const core::ecs::TransformComponent* tr = nullptr;

                        if (!tryGetLocalPlayerTransform(*mWorld, e, tr)) {
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
            // Отпускание клавиш.
            else if (const auto* keyReleased = event.getIf<sf::Event::KeyReleased>()) {
                mAircraftControlSystem->onKeyEvent(keyReleased->code, false);
            }
            // Потеря фокуса: сбрасываем ввод, чтобы не залипали клавиши при Alt-Tab.
            // Также сбрасываем состояние управления самолетом.
            else if (event.is<sf::Event::FocusLost>()) {
                mAircraftControlSystem->resetState();
            }
            // Изменение размера окна.
            else if (const auto* resized = event.getIf<sf::Event::Resized>()) {
                const sf::Vector2u newSize{resized->size.x, resized->size.y};
                // При минимизации окно может сообщить 0x0. Не создаём/не применяем невалидный view.
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

        // Применяем отложенный Alt+Enter toggle одним действием, вне цикла pollEvent.
        if (mWindow.isOpen() && mWindowModeManager.applyPending(mWindow)) {
            mWindow.setKeyRepeatEnabled(false);

            applyEngineSettingsToWindow();
            mViewManager.onResize(mWindow.getSize());

            // Безопасно сбросить ввод: при пересоздании окна возможны "залипания" состояния.
            if (mAircraftControlSystem) {
                mAircraftControlSystem->resetState();
            }

            // Примечание:
            //  - mode сохраняем всегда (Alt+Enter).
            //  - width/height сохраняем только в Windowed, чтобы не "затирать" желаемый размер
            //    случайным desktop size из borderless/fullscreen.
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

    // Здесь dt — фиксированный шаг (FIXED_TIME_STEP), который пришёл из игрового цикла
    // и был "разрулен" TimeService (через shouldUpdate). ECS-системы не знают о том,
    // сколько реального времени прошло между кадрами, они видят стабильный шаг логики.
    void Game::update(const sf::Time& dt) {
        const float dtSeconds = dt.asSeconds();
        assert(dtSeconds > 0.0f); // фиксированный шаг должен быть положительным
        mWorld->update(dtSeconds); // обновляем все ECS-системы
        updateCamera();
        mWorld->flushDestroyed();
    }

    void Game::updateCamera() {
        core::ecs::Entity e{};
        const core::ecs::TransformComponent* tr = nullptr;

        const bool foundPlayer = tryGetLocalPlayerTransform(*mWorld, e, tr);
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

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        // Важно для Profile: если overlay выключен — не тратим CPU на форматирование extra-строк.
        if (mDebugOverlay->isEnabled()) {
            mDebugOverlay->clearExtraText();

            // Background line (Debug/Profile)
            {
                std::array<char, 256> extraBuffer{};
                const std::size_t extraSize = utils::formatBackgroundStatsLine(
                    extraBuffer.data(), extraBuffer.size(), mBackgroundRenderer);

                // Если обрезали строку — увеличь буфер или сократи формат.
                assert(extraSize < extraBuffer.size() &&
                       "Game::renderUiPass: background debug line truncated. "
                       "Increase extraBuffer.");

                if (extraSize > 0) {
                    mDebugOverlay->appendExtraLine(std::string_view(extraBuffer.data(), extraSize));
                }
            }

#if !defined(NDEBUG)
            // Camera line (Debug only)
            {
                core::ecs::Entity e{};
                const core::ecs::TransformComponent* tr = nullptr;

                if (tryGetLocalPlayerTransform(*mWorld, e, tr)) {
                    std::array<char, 256> camBuf{};
                    const auto& vw = mViewManager.getWorldView();
                    const sf::Vector2f off = mViewManager.getCameraOffset();

                    const std::size_t camSize = utils::formatCameraStatsLine(
                        camBuf.data(), camBuf.size(), tr->position.y, vw.getCenter().y,
                        vw.getSize().y, off.y, mViewManager.getCameraCenterYMax());

                    if (camSize > 0) {
                        mDebugOverlay->appendExtraLine(std::string_view(camBuf.data(), camSize));
                    }
                }
            }
#endif

            if (mSpatialIndexSystem) {
                std::array<char, 256> streamBuf{};
                const auto& worldView = mViewManager.getWorldView();
                const std::int32_t chunkSize = mSpatialIndexSystem->index().chunkSizeWorld();
                const auto origin = mSpatialIndexSystem->index().windowOrigin();
                const std::size_t streamSize = utils::formatStreamingStatsLine(
                    streamBuf.data(), streamBuf.size(), worldView, origin, chunkSize);
                if (streamSize > 0) {
                    mDebugOverlay->appendExtraLine(std::string_view(streamBuf.data(), streamSize));
                }

                const sf::Vector2f viewCenter = worldView.getCenter();
                const core::spatial::ChunkCoord viewChunk = core::spatial::worldToChunk(
                    core::spatial::WorldPosf{viewCenter.x, viewCenter.y}, chunkSize);
                const auto health = mSpatialIndexSystem->index().debugCellHealthForChunk(viewChunk);

                std::array<char, 256> healthBuf{};
                const std::size_t healthSize = utils::formatCellHealthLine(
                    healthBuf.data(), healthBuf.size(), health.maxCellLen, health.sumCellLen,
                    health.dupApprox, health.loaded);
                if (healthSize > 0) {
                    mDebugOverlay->appendExtraLine(std::string_view(healthBuf.data(), healthSize));
                }
            }

            {
                core::ecs::Entity e{};
                const core::ecs::TransformComponent* tr = nullptr;

                if (tryGetLocalPlayerTransform(*mWorld, e, tr)) {
                    const sf::View& worldView = mViewManager.getWorldView();
                    const sf::Vector2f viewSize = worldView.getSize();
                    const sf::Vector2f viewCenter = worldView.getCenter();
                    const sf::Vector2f topLeft{viewCenter.x - viewSize.x * 0.5f,
                                               viewCenter.y - viewSize.y * 0.5f};
                    core::spatial::Aabb2 viewAabb{};
                    viewAabb.minX = topLeft.x;
                    viewAabb.minY = topLeft.y;
                    viewAabb.maxX = topLeft.x + viewSize.x;
                    viewAabb.maxY = topLeft.y + viewSize.y;

                    const auto* spatialId = mWorld->tryGetComponent<core::ecs::SpatialIdComponent>(e);
                    const bool hasSpatialId = (spatialId != nullptr);
                    const std::uint32_t spatialIdVal = hasSpatialId
                        ? static_cast<std::uint32_t>(spatialId->id)
                        : 0u;
                    const bool hasDirty = mWorld->hasTag<core::ecs::SpatialDirtyTag>(e);
                    const bool hasStreamedOut =
                        mWorld->hasTag<core::ecs::SpatialStreamedOutTag>(e);
                    const core::spatial::Aabb2 lastAabb =
                        hasSpatialId ? spatialId->lastAabb : core::spatial::Aabb2{};
                    const auto* sprite = mWorld->tryGetComponent<core::ecs::SpriteComponent>(e);
                    core::spatial::Aabb2 newAabb = lastAabb;
                    if (sprite != nullptr) {
                        const float rotationDegrees = tr->rotationDegrees;
                        if (rotationDegrees == 0.f) {
                            newAabb = core::ecs::render::computeSpriteAabbNoRotation(
                                tr->position, sprite->origin, sprite->scale, sprite->textureRect);
                        } else {
                            const float radians = rotationDegrees * core::utils::kDegToRad;
                            const float cachedSin = std::sin(radians);
                            const float cachedCos = std::cos(radians);
                            newAabb = core::ecs::render::computeSpriteAabbRotated(
                                tr->position, sprite->origin, sprite->scale, sprite->textureRect,
                                cachedSin, cachedCos);
                        }
                    }
                    const bool fineCullPass = hasSpatialId
                        ? core::spatial::intersectsInclusive(lastAabb, viewAabb)
                        : false;
                    const bool inQuery = (hasSpatialId && mSpatialIndexSystem)
                        ? mSpatialIndexSystem->index().debugWasInLastQuery(spatialId->id)
                        : false;
                    // predictedVisible != "реально отрисовано": прогноз по условиям UI-pass.
                    const bool predictedVisible =
                        (sprite != nullptr) && inQuery && fineCullPass && !hasStreamedOut;

                    std::array<char, 256> watchStateBuf{};
                    const std::size_t watchStateSize = utils::formatPlayerWatchStateLine(
                        watchStateBuf.data(), watchStateBuf.size(), core::ecs::toUint(e),
                        hasSpatialId, spatialIdVal, hasDirty, hasStreamedOut, tr->position);
                    if (watchStateSize > 0) {
                        mDebugOverlay->appendExtraLine(
                            std::string_view(watchStateBuf.data(), watchStateSize));
                    }

                    std::array<char, 256> watchVisBuf{};
                    const std::size_t watchVisSize = utils::formatPlayerWatchVisibilityLine(
                        watchVisBuf.data(), watchVisBuf.size(), lastAabb, newAabb, fineCullPass,
                        inQuery, predictedVisible);
                    if (watchVisSize > 0) {
                        mDebugOverlay->appendExtraLine(
                            std::string_view(watchVisBuf.data(), watchVisSize));
                    }
                }
            }
        }
#endif

        mDebugOverlay->prepareFrame(*mWorld);
        mDebugOverlay->draw(mWindow);
    }

} // namespace game::skyguard