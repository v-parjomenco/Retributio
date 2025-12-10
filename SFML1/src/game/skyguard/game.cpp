#include "pch.h"

#include "game/skyguard/game.h"
#include <cassert>

#include "core/config/engine_settings.h"
#include "core/config/loader/debug_overlay_loader.h"
#include "core/config/loader/engine_settings_loader.h"
#include "core/debug/debug_config.h"
#include "core/ecs/systems/debug_overlay_system.h"
#include "core/ecs/systems/input_system.h"
#include "core/ecs/systems/lock_system.h"
#include "core/ecs/systems/movement_system.h"
#include "core/ecs/systems/render_system.h"
#include "core/ecs/systems/scaling_system.h"
#include "core/log/log_macros.h"
#include "core/resources/paths/resource_paths.h"
#include "core/time/time_config.h"

#include "game/skyguard/config/loader/config_loader.h"
#include "game/skyguard/config/loader/window_config_loader.h"
#include "game/skyguard/config/window_config.h"
#include "game/skyguard/ecs/components/player_config_component.h"
#include "game/skyguard/ecs/systems/player_init_system.h"

// Движковые конфиги (vsync, frame limit и т.п.).
namespace cfg       = ::core::config;
// Движковые настройки времени (fixed timestep и т.п.).
namespace timecfg   = ::core::time;
// Debug-флаги и хоткеи (overlay, hold on exit и т.п.).
namespace dbg = ::core::debug;
// Специфические игровые конфиги/blueprints для SkyGuard (player.json, window и т.п.).
namespace skycfg    = ::game::skyguard::config;

namespace game::skyguard {

    Game::Game() {
        // ----------------------------------------------------------------------------------------
        // Загружаем конфиг окна SkyGuard из JSON (skyguard_game.json)
        // ----------------------------------------------------------------------------------------
        const skycfg::WindowConfig windowCfg =
            skycfg::loadWindowConfig("assets/game/skyguard/config/skyguard_game.json");

        // Создаём окно с настройками из JSON.
        mWindow.create(sf::VideoMode({windowCfg.width, windowCfg.height}), windowCfg.title);

        // ----------------------------------------------------------------------------------------
        // Загружаем движковые настройки рендеринга (EngineSettings)
        // ----------------------------------------------------------------------------------------
        cfg::EngineSettings engineSettings =
            cfg::loadEngineSettings("assets/core/config/engine_settings.json");

        mWindow.setVerticalSyncEnabled(engineSettings.vsyncEnabled);
        if (!engineSettings.vsyncEnabled) {
            mWindow.setFramerateLimit(engineSettings.frameLimit);
        }

        // Логируем итоговые настройки рендеринга один раз при запуске игры.
        LOG_INFO(core::log::cat::Gameplay,
                 "[EngineSettings]\nVSync: {}, frameLimit: {}{}",
                 (engineSettings.vsyncEnabled ? "enabled" : "disabled"),
                 engineSettings.frameLimit,
                 (engineSettings.vsyncEnabled
                      ? " (VSync enabled, frameLimit ignored)."
                      : " (VSync disabled, frameLimit applied)."));

        // ----------------------------------------------------------------------------------------
        // Инициализация ресурсного слоя (реестр ресурсов + fallback-ресурсы)
        // ----------------------------------------------------------------------------------------

        // Важно: initResources() должен быть вызван до initWorld(),
        // чтобы ResourceManager уже знал о fallback-ресурсах и реестр
        // был загружен.

        initResources();

        // ----------------------------------------------------------------------------------------
        // Создаём ECS-мир и игровые сущности SkyGuard
        // ----------------------------------------------------------------------------------------
        initWorld();
    }

    void Game::initResources() {
        // Загружаем реестр ресурсов из JSON.
        core::resources::paths::ResourcePaths::loadFromJSON(
            "assets/game/skyguard/config/resources.json");
        // Настраиваем fallback-ресурсы на уровне ResourceManager.
        //
        // Сейчас гарантированно есть только один игровой шрифт — FontID::Default.
        // Его и используем как "последний рубеж" на случай битых путей или ошибочных ID.
        mResources.setMissingFontFallback(core::resources::ids::FontID::Default);

        // Для текстур и звуков пока специально не выставляем fallback:
        //  - у нас ещё нет отдельной текстуры-заглушки (фиолетовый квадрат) с собственным TextureID;
        //  - звуки в текущем билде не используются.
        //
        // Как только появится отдельная fallback-текстура:
        //  1) добавляем её в enum TextureID (например, TextureID::MissingTexture);
        //  2) регистрируем в resources.json;
        //  3) включаем строку ниже:
        //
        // mResources.setMissingTextureFallback(core::resources::ids::TextureID::MissingTexture);
        //
        // Аналогично для звуков, когда появится базовый "beep"/"ui_click" и др.:
        //
        // mResources.setMissingSoundFallback(core::resources::ids::SoundID::SomeDefault);
    }

    // initWorld() без try / catch — все исключения уходят наверх в main()
    void Game::initWorld() {

        // Создаём blueprint игрока (data-driven, собранный из properties)
        auto playerCfg =
            skycfg::ConfigLoader::loadPlayerConfig("assets/game/skyguard/config/player.json");

        // Создаём сущность игрока
        mPlayerEntity = mWorld.createEntity();

        // Добавляем компонент с конфигурацией игрока из JSON (playerCfg) в ECS-мир.
        // PlayerInitSystem при первом апдейте создаст остальные компоненты
        // (Sprite, Transform и т.д.).
        mWorld.addComponent(mPlayerEntity,
                            game::skyguard::ecs::PlayerConfigComponent{playerCfg});

        // ------------------------------------------------------------------------------------
        // Подключаем ECS-системы
        // ------------------------------------------------------------------------------------
        // Передаём в PlayerInitSystem базовый размер view из текущего окна,
        // чтобы якоря и базовые размеры масштабирования были согласованы с реальным окном.
        const sf::Vector2f baseViewSize = mWindow.getView().getSize();
        mWorld.addSystem<game::skyguard::ecs::PlayerInitSystem>(mResources, baseViewSize);

        mWorld.addSystem<core::ecs::MovementSystem>();
        mWorld.addSystem<core::ecs::RenderSystem>();
        // Эти системы требуют прямого доступа (onResize, onKeyEvent),
        // поэтому сохраняем указатели.
        mScalingSystem  = &mWorld.addSystem<core::ecs::ScalingSystem>();
        mLockSystem     = &mWorld.addSystem<core::ecs::LockSystem>();
        mInputSystem    = &mWorld.addSystem<core::ecs::InputSystem>();
        mDebugOverlay   = &mWorld.addSystem<core::ecs::DebugOverlaySystem>();

        // Привязываем overlay к сервису времени и шрифту (через ResourceManager).
        if (mDebugOverlay) {
            const sf::Font& font = mResources.getFont(core::resources::ids::FontID::Default).get();

            mDebugOverlay->bind(mTime, font);

            // Грузим конфиг для DebugOverlay.
            const auto overlayCfg =
                cfg::loadDebugOverlayBlueprint("assets/core/config/debug_overlay.json");

            // Применяем стиль.
            mDebugOverlay->applyTextProperties(overlayCfg.text);

            // Итоговое состояние: overlay включён, только если:
            //  - конфиг debug_overlay.json его не отключил;
            //  - сборка разрешает overlay (Debug=true, Release=false) через SHOW_FPS_OVERLAY.
            mDebugOverlay->setEnabled(overlayCfg.enabled && dbg::SHOW_FPS_OVERLAY);
        }
    }

    /**
     * @brief Главный игровой цикл SkyGuard.
     *
     * Структура кадра:
     *  1) mTime.tick() — измеряем время кадра, обновляем FPS и накопление для fixed-step;
     *  2) processEvents() — обрабатываем все события окна/ввода;
     *  3) while (shouldUpdate(FIXED_TIME_STEP)) — выполняем 0..N фиксированных шагов логики;
     *  4) render() — один кадр отрисовки текущего состояния мира.
     *
     * Логика времени:
     *  - TimeService управляет тем, сколько раз за кадр будет вызван update();
     *  - dt для мира — FIXED_TIME_STEP (compile-time константа движка);
     *  - пауза и timeScale (когда будут использоваться) настраиваются через TimeService,
     *    но сам цикл остаётся таким же.
     */
    void Game::run() {
        assert(mWindow.isOpen()); // проверка, что окно открылось

        const sf::Time fixedTimeStep = timecfg::FIXED_TIME_STEP;

        while (mWindow.isOpen()) {
            // Обновляем время кадра (raw dt, scaled dt, FPS/метрики).
            mTime.tick();
            // Обрабатываем события окна и ввода.
            processEvents();
            // Выполняем один или несколько фиксированных шагов логики.
            while (mTime.shouldUpdate(fixedTimeStep)) {
                update(fixedTimeStep);
            }
            // Отрисовываем текущее состояние мира.
            render();
        }
    }

    void Game::processEvents() {

        while (const std::optional<sf::Event> event = mWindow.pollEvent()) {

            // Обработка закрытия окна
            if (event->is<sf::Event::Closed>()) {
                mWindow.close();
            }

            // Обработка нажатия и отпускания клавиш
            else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                if (mInputSystem) {
                    mInputSystem->onKeyEvent(keyPressed->code, true);
                }
                if (mDebugOverlay && keyPressed->code == dbg::HOTKEY_TOGGLE_OVERLAY) {
                    mDebugOverlay->setEnabled(!mDebugOverlay->isEnabled());
                }
            } else if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>()) {
                if (mInputSystem) {
                    mInputSystem->onKeyEvent(keyReleased->code, false);
                }
            }

            // Обработка изменения размера окна
            else if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                sf::Vector2f newSize(static_cast<float>(resized->size.x),
                                     static_cast<float>(resized->size.y));
                sf::View newView({newSize.x / 2.f, newSize.y / 2.f}, {newSize.x, newSize.y});
                // Обновляем View окна, чтобы другие поведения и отрисовка видели актуальный вид.
                mWindow.setView(newView);
                if (mScalingSystem) {
                    mScalingSystem->onResize(mWorld, newView); // безопасный вызов (null-check)
                }
                if (mLockSystem) {
                    mLockSystem->onResize(mWorld, newView); // безопасный вызов (null-check)
                }
            }
        }
    }

    // Здесь dt — фиксированный шаг (FIXED_TIME_STEP), который пришёл из игрового цикла
    // и был "разрулен" TimeService (через shouldUpdate). ECS-системы не знают о том,
    // сколько реального времени прошло между кадрами, они видят стабильный шаг логики.
    void Game::update(const sf::Time& dt) {
        assert(dt.asSeconds() > 0);    // время обновления должно быть положительным
        mWorld.update(dt.asSeconds()); // обновляем все ECS-системы
    }

    void Game::render() {
        mWindow.clear();
        mWorld.render(mWindow); // отрисовываем ECS-мир
        mWindow.display();
    }

} // namespace game::skyguard