#include "pch.h"

#include "game/skyguard/game.h"

#include <cassert>
#include <cstdint>
#include <optional>
#include <stdexcept>

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

#include "game/skyguard/config/config_paths.h"
#include "game/skyguard/config/loader/config_loader.h"
#include "game/skyguard/config/loader/window_config_loader.h"
#include "game/skyguard/config/window_config.h"
#include "game/skyguard/ecs/systems/player_init_system.h"

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
    #include "game/skyguard/dev/stress_scene.h"
#endif

// Движковые конфиги (vsync, frame limit и т.п.).
namespace cfg = ::core::config;
// Движковые настройки времени (fixed timestep и т.п.).
namespace timecfg = ::core::time;
// Debug-флаги и хоткеи (overlay, hold on exit и т.п.).
namespace dbg = ::core::debug;
// Специфические игровые конфиги/blueprints для SkyGuard (player.json, window и т.п.).
namespace skycfg = ::game::skyguard::config;
// Централизованное хранилище путей к JSON-конфигам
namespace skycfg_paths = ::game::skyguard::config::paths;

namespace game::skyguard {

    Game::Game() {
        // ----------------------------------------------------------------------------------------
        // Загружаем конфиг окна SkyGuard из JSON (skyguard_game.json)
        // ----------------------------------------------------------------------------------------
        const skycfg::WindowConfig windowCfg =
            skycfg::loadWindowConfig(skycfg_paths::SKYGUARD_GAME);

        // Создаём окно с настройками из JSON.
        mWindow.create(sf::VideoMode({windowCfg.width, windowCfg.height}), windowCfg.title);

        // Если окно не открылось — это фатально. (SFML обычно не кидает исключения здесь.)
        if (!mWindow.isOpen()) {
            LOG_ERROR(core::log::cat::Engine,
                      "Failed to create main window ({}x{}).",
                      windowCfg.width,
                      windowCfg.height);
            throw std::runtime_error("Failed to create main window");
        }

        // ----------------------------------------------------------------------------------------
        // Загружаем движковые настройки рендеринга (EngineSettings)
        // ----------------------------------------------------------------------------------------
        const cfg::EngineSettings engineSettings =
            cfg::loadEngineSettings(skycfg_paths::ENGINE_SETTINGS);

        // Политика применения:
        //  - VSync ON  => frameLimit должен быть выключен (0), чтобы не было двойного ограничения.
        //  - VSync OFF => применяем frameLimit из конфига.
        mWindow.setVerticalSyncEnabled(engineSettings.vsyncEnabled);
        if (!engineSettings.vsyncEnabled) {
            mWindow.setFramerateLimit(engineSettings.frameLimit);
        } else {
            mWindow.setFramerateLimit(0);
        }

        // Логируем итоговые настройки рендеринга один раз при запуске игры.
        LOG_INFO(core::log::cat::Gameplay,
                 "[EngineSettings] VSync: {}, frameLimit: {}{}",
                 (engineSettings.vsyncEnabled ? "enabled" : "disabled"),
                 engineSettings.frameLimit,
                 (engineSettings.vsyncEnabled
                      ? " (VSync enabled, frameLimit ignored)."
                      : " (VSync disabled, frameLimit applied)."));

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
    }

    void Game::initResources() {
        // Загружаем реестр ресурсов из JSON.
        // Это критичный конфиг: если он сломан — игра не имеет смысла продолжать.
        core::resources::paths::ResourcePaths::loadFromJSON(skycfg_paths::RESOURCES);

        // Настраиваем fallback-ресурсы на уровне ResourceManager.
        //
        // Сейчас гарантированно есть только один игровой шрифт — FontID::Default.
        // Его и используем как "последний рубеж" на случай битых путей или ошибочных ID.
        mResources.setMissingFontFallback(core::resources::ids::FontID::Default);

        // Для текстур и звуков пока специально не выставляем fallback:
        //  - у нас ещё нет отдельной текстуры-заглушки (фиолетовый квадрат)
        //    с собственным TextureID;
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

    // initWorld() без try/catch — все исключения уходят наверх в main()
    void Game::initWorld() {
        // Загружаем blueprint игрока (data-driven).
        auto playerCfg = skycfg::ConfigLoader::loadPlayerConfig(skycfg_paths::PLAYER);

        // Базовый размер (design/reference) и фактический стартовый размер окна сейчас совпадают.
        // Если позже захочешь "design resolution" отдельно — прокинем baseViewSize из конфига.
        const sf::Vector2f baseViewSize = mWindow.getView().getSize();
        const sf::Vector2f initialViewSize = baseViewSize;

        std::vector<game::skyguard::config::blueprints::PlayerBlueprint> players;
        players.emplace_back(std::move(playerCfg));

        // ----------------------------------------------------------------------------------------
        // Подключаем ECS-системы (порядок важен для update/render)
        // ----------------------------------------------------------------------------------------

        // PlayerInitSystem сам создаёт сущности на первом тике и больше не работает.
        mWorld.addSystem<game::skyguard::ecs::PlayerInitSystem>(
            mResources, baseViewSize, initialViewSize, std::move(players));

        // ВАЖНО: Input должен обновляться ДО Movement, т.к. Input пишет VelocityComponent,
        // а Movement читает VelocityComponent в этом же тике.
        mInputSystem    = &mWorld.addSystem<core::ecs::InputSystem>();
        mWorld.addSystem<core::ecs::MovementSystem>();

        // Эти системы требуют прямого доступа (onResize, onKeyEvent),
        // поэтому сохраняем указатели.
        mRenderSystem   = &mWorld.addSystem<core::ecs::RenderSystem>(mResources);
        mScalingSystem  = &mWorld.addSystem<core::ecs::ScalingSystem>();
        mLockSystem     = &mWorld.addSystem<core::ecs::LockSystem>();
        mDebugOverlay   = &mWorld.addSystem<core::ecs::DebugOverlaySystem>();

        // Привязываем overlay к сервису времени и шрифту (через ResourceManager).
        // Важно: ресурсы резолвим один раз при старте, не в hot-path.
        {
            const sf::Font& font = mResources.getFont(core::resources::ids::FontID::Default).get();

            mDebugOverlay->bind(mTime, font);
            mDebugOverlay->setRenderSystem(mRenderSystem);

            // Грузим конфиг для DebugOverlay (dev/косметика, не должен валить игру).
            const auto overlayCfg = cfg::loadDebugOverlayBlueprint(skycfg_paths::DEBUG_OVERLAY);

            // Применяем стиль.
            mDebugOverlay->applyTextProperties(overlayCfg.text);
            mDebugOverlay->applyRuntimeProperties(overlayCfg.runtime);

            // Политика дефолтного состояния overlay при старте:
            //  - overlayCfg.enabled может ОТКЛЮЧИТЬ overlay по умолчанию;
            //  - dbg::SHOW_FPS_OVERLAY — compile-time флаг (Debug/Profile: true, Release: false).
            //
            // ВАЖНО: при формуле ниже JSON не может "включить overlay в Release на старте",
            // потому что compile-time флаг сильнее. Но хоткей F3 (dbg::HOTKEY_TOGGLE_OVERLAY)
            // всё равно позволяет включить overlay в рантайме в любой сборке.
            mDebugOverlay->setEnabled(overlayCfg.enabled && dbg::SHOW_FPS_OVERLAY);
        }

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        // DEV/PROFILE-only: стресс-сцена через ENV.
        game::skyguard::dev::trySpawnStressSpritesFromEnv(
            mWorld, mResources, core::resources::ids::TextureID::Player);
#endif
    }

    void Game::run() {
        assert(mWindow.isOpen()); // проверка, что окно открылось

        const sf::Time fixedTimeStep = timecfg::FIXED_TIME_STEP;

        LOG_INFO(core::log::cat::Gameplay, "Game loop started");

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        std::uint64_t frameCount = 0;
#endif

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

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
            // Rate-limited debug: не спамим логом.
            if (++frameCount % 600ULL == 0ULL) {
                LOG_DEBUG(core::log::cat::Performance,
                          "FPS: {:.1f} (frame {})",
                          mTime.getSmoothedFps(),
                          frameCount);
            }
#endif
        }
    }

    void Game::processEvents() {
        while (const std::optional<sf::Event> event = mWindow.pollEvent()) {
            // Закрытие окна.
            if (event->is<sf::Event::Closed>()) {
                mWindow.close();
            }
            // Нажатие клавиш.
            else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                mInputSystem->onKeyEvent(keyPressed->code, true);

                // Debug overlay toggle: хоткей работает во всех сборках (Debug/Release/Profile).
                // Политика: хоткей ВСЕГДА активен, независимо от дефолтного состояния overlay
                // при старте.
                if (keyPressed->code == dbg::HOTKEY_TOGGLE_OVERLAY) {
                    mDebugOverlay->setEnabled(!mDebugOverlay->isEnabled());
                }
            }
            // Отпускание клавиш.
            else if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>()) {
                mInputSystem->onKeyEvent(keyReleased->code, false);
            }

            // Потеря фокуса: сбрасываем ввод, чтобы не залипали клавиши при Alt-Tab/клике мимо окна.
            else if (event->is<sf::Event::FocusLost>()) {
                mInputSystem->resetState();
            }

            // Изменение размера окна.
            else if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                const sf::Vector2f newSize(static_cast<float>(resized->size.x),
                                           static_cast<float>(resized->size.y));

                // При минимизации окно может сообщить 0x0. Не создаём/не применяем invalid view.
                if (newSize.x <= 0.0f || newSize.y <= 0.0f) {
                    continue;
                }

                const sf::View newView({newSize.x * 0.5f, newSize.y * 0.5f},
                                       {newSize.x, newSize.y});

                // Обновляем View окна, чтобы другие поведения и отрисовка видели актуальный вид.
                mWindow.setView(newView);

                // Пересчёт UI/привязок.
                mScalingSystem->onResize(mWorld, newView);
                mLockSystem->onResize(mWorld, newView);
            }
        }
    }

    // Здесь dt — фиксированный шаг (FIXED_TIME_STEP), который пришёл из игрового цикла
    // и был "разрулен" TimeService (через shouldUpdate). ECS-системы не знают о том,
    // сколько реального времени прошло между кадрами, они видят стабильный шаг логики.
    void Game::update(const sf::Time& dt) {
        const float dtSeconds = dt.asSeconds();
        assert(dtSeconds > 0.0f); // фиксированный шаг должен быть положительным

        mWorld.update(dtSeconds); // обновляем все ECS-системы
    }

    void Game::render() {
        mWindow.clear();
        mWorld.render(mWindow); // отрисовываем ECS-мир
        mWindow.display();
    }

} // namespace game::skyguard