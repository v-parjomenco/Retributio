#include "pch.h"

#include "game/skyguard/game.h"
#include <cassert>
#include <iostream>

#include "core/config/engine_settings.h"
#include "core/config/loader/debug_overlay_loader.h"
#include "core/config/loader/engine_settings_loader.h"
#include "core/ecs/systems/debug_overlay_system.h"
#include "core/ecs/systems/input_system.h"
#include "core/ecs/systems/lock_system.h"
#include "core/ecs/systems/movement_system.h"
#include "core/ecs/systems/render_system.h"
#include "core/ecs/systems/scaling_system.h"
#include "core/resources/paths/resource_paths.h"
#include "core/utils/message.h"

#include "game/skyguard/config/loader/config_loader.h"
#include "game/skyguard/config/loader/window_config_loader.h"
#include "game/skyguard/config/window_config.h"
#include "game/skyguard/ecs/components/player_config_component.h"
#include "game/skyguard/ecs/systems/player_init_system.h"

// Движковые дефолты: fixed timestep, рендер-политики, debug overlay, holdOnExit.
namespace cfg = ::core::config;
// Специфические игровые конфиги/blueprints для SkyGuard (player.json, window и т.п.).
namespace skycfg = ::game::skyguard::config;

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
        core::utils::message::logDebug(
            "[EngineSettings]\nVSync: " +
            std::string(engineSettings.vsyncEnabled ? "enabled" : "disabled") +
            ", frameLimit: " + std::to_string(engineSettings.frameLimit) +
            (engineSettings.vsyncEnabled ? " (VSync включён, frameLimit игнорируется)."
                                         : " (VSync выключён, frameLimit применяется)."));

        // ----------------------------------------------------------------------------------------
        // Загружаем ресурсы/пути из JSON
        // ----------------------------------------------------------------------------------------
        core::resources::paths::ResourcePaths::loadFromJSON(
            "assets/game/skyguard/config/resources.json");

        initWorld(); // создаём ECS-мир и сущности
    }

    void Game::initWorld() {
        try {
            // Создаём blueprint игрока (data-driven, собранный из properties)
            auto playerCfg =
                skycfg::ConfigLoader::loadPlayerConfig("assets/game/skyguard/config/player.json");

            // Создаём сущность игрока
            mPlayerEntity = mWorld.createEntity();

            // Добавляем компонент с конфигурацией игрока из JSON (playerCfg) в ECS-мир
            // PlayerInitSystem при первом апдейте создаст остальные компоненты
            // (Sprite, Transform и т.д.)
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
            // поэтому сохраняем указатели
            mScalingSystem = &mWorld.addSystem<core::ecs::ScalingSystem>();
            mLockSystem = &mWorld.addSystem<core::ecs::LockSystem>();
            mInputSystem = &mWorld.addSystem<core::ecs::InputSystem>();
            mDebugOverlay = &mWorld.addSystem<core::ecs::DebugOverlaySystem>();

            // Привязываем overlay к сервису времени и шрифту (через ResourceManager)
            if (mDebugOverlay) {
                const sf::Font& font =
                    mResources.getFont(core::resources::ids::FontID::Default).get();

                mDebugOverlay->bind(mTime, font);

                // Грузим конфиг для DebugOverlay
                const auto overlayCfg =
                    cfg::loadDebugOverlayBlueprint("assets/core/config/debug_overlay.json");

                // Применяем стиль
                mDebugOverlay->applyTextProperties(overlayCfg.text);

                // Итоговое состояние: overlay включён, только если:
                //  - конфиг debug_overlay.json его не отключил;
                //  - сборка разрешает overlay (Debug=true, Release=false) через SHOW_FPS_OVERLAY.
                mDebugOverlay->setEnabled(overlayCfg.enabled && cfg::SHOW_FPS_OVERLAY);
            }
        } catch (const std::exception& e) {
            // Кросс-платформенная обработка ошибок
#ifdef _WIN32
            core::utils::message::showError(std::string("Ошибка при инициализации ECS: ") +
                                            e.what());
            core::utils::message::holdOnExit();
#else
            std::cerr << "Ошибка при инициализации ECS: " << e.what() << std::endl;
#endif
            mWorld = std::move(core::ecs::World()); // fallback: создаём безопасный пустой мир
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

        const sf::Time fixedTimeStep = cfg::FIXED_TIME_STEP;

        while (mWindow.isOpen()) {
            // 1. Обновляем время кадра (raw dt, scaled dt, FPS/метрики).
            mTime.tick();
            // 2. Обрабатываем события окна и ввода.
            processEvents();
            // 3. Выполняем один или несколько фиксированных шагов логики.
            while (mTime.shouldUpdate(fixedTimeStep)) {
                update(fixedTimeStep);
            }
            // 4. Отрисовываем текущее состояние мира.
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
                if (mDebugOverlay && keyPressed->code == cfg::HOTKEY_TOGGLE_OVERLAY) {
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
                // Обновляем View окна, чтобы другие поведения и отрисовка видели актуальный вид
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
