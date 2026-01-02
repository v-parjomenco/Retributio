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
#include "core/ecs/systems/lock_system.h"
#include "core/ecs/systems/movement_system.h"
#include "core/ecs/systems/render_system.h"
#include "core/ecs/systems/scaling_system.h"
#include "core/ecs/systems/spatial_index_system.h"
#include "core/log/log_macros.h"
#include "core/time/time_config.h"

#include "game/skyguard/config/config_paths.h"
#include "game/skyguard/config/loader/config_loader.h"
#include "game/skyguard/config/loader/window_config_loader.h"
#include "game/skyguard/config/window_config.h"
#include "game/skyguard/ecs/systems/player_init_system.h"
#include "game/skyguard/ecs/systems/aircraft_control_system.h"

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
        mEngineSettings = cfg::loadEngineSettings(skycfg_paths::ENGINE_SETTINGS);

        // Политика применения:
        //  - VSync ON  => frameLimit выключен (0), избегаем двойного ограничения.
        //  - VSync OFF => применяем frameLimit из конфига.
        mWindow.setVerticalSyncEnabled(mEngineSettings.vsyncEnabled);
        if (!mEngineSettings.vsyncEnabled) {
            mWindow.setFramerateLimit(mEngineSettings.frameLimit);
        } else {
            mWindow.setFramerateLimit(0);
        }

        // Логируем итоговые настройки рендеринга один раз при запуске игры.
        LOG_INFO(core::log::cat::Gameplay, "[EngineSettings] VSync: {}, frameLimit: {}{}",
                 (mEngineSettings.vsyncEnabled ? "enabled" : "disabled"),
                  mEngineSettings.frameLimit,
                  (mEngineSettings.vsyncEnabled ? " (VSync enabled, frameLimit ignored)."
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
        mResources.loadRegistryFromJson(skycfg_paths::RESOURCES);

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
    }

    // initWorld() без try/catch — все исключения уходят наверх в main()
    void Game::initWorld() {
        // Загружаем blueprint игрока (data-driven).
        auto playerCfg = skycfg::ConfigLoader::loadPlayerConfig(skycfg_paths::PLAYER);

        // Базовый размер (design/reference) и фактический стартовый размер окна сейчас совпадают.
        // Если позже захочешь "design resolution" отдельно — прокинем baseViewSize из конфига.
        const sf::Vector2f baseViewSize = mWindow.getView().getSize();

        // Инвариант: после успешного create() view всегда валиден (ненулевой размер).
        // Проверка нужна только для отлова багов в SFML или memory corruption.
        assert((baseViewSize.x > 0.f) && (baseViewSize.y > 0.f) &&
               "Game::initWorld: window view has invalid size after create(). "
               "This indicates SFML bug or memory corruption.");

        const sf::Vector2f initialViewSize = baseViewSize;

        std::vector<game::skyguard::config::blueprints::PlayerBlueprint> players;
        players.emplace_back(std::move(playerCfg));

        // ----------------------------------------------------------------------------------------
        // Подключаем ECS-системы (порядок важен для update/render)
        // ----------------------------------------------------------------------------------------

        // PlayerInitSystem сам создаёт сущности на первом тике и больше не работает.
        mWorld.addSystem<game::skyguard::ecs::PlayerInitSystem>(
            mResources, baseViewSize, initialViewSize, std::move(players));

        // ВАЖНО: AircraftControl должен обновляться ДО Movement, т.к. пишет VelocityComponent,
        // а Movement читает VelocityComponent в этом же тике.
        mAircraftControlSystem = &mWorld.addSystem<game::skyguard::ecs::AircraftControlSystem>();
        mWorld.addSystem<core::ecs::MovementSystem>();
        auto& spatialSystem =
            mWorld.addSystem<core::ecs::SpatialIndexSystem>(mEngineSettings.spatialCellSize);

        // 1. Создаем систему рендеринга конструктором по умолчанию (без аргументов)
        auto& renderSys = mWorld.addSystem<core::ecs::RenderSystem>();
        // 2. Привязываем зависимости через bind()
        // Передаем адреса (&), так как bind принимает указатели
        renderSys.bind(&spatialSystem.index(), &mResources);
        // 3. Сохраняем указатель
        mRenderSystem = &renderSys;

        // Эти системы требуют прямого доступа (onResize, onKeyEvent),
        // поэтому сохраняем указатели.
        //mRenderSystem =
        //    &mWorld.addSystem<core::ecs::RenderSystem>(mResources, spatialSystem.index());
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

        // ----------------------------------------------------------------------------------------
        // Spiral of death prevention: вычисляем max апдейтов за кадр из инварианта TimeService.
        // Формула выводится из константы TimeService::kMaxAccumulatedSeconds и fixed timestep.
        // При текущих значениях (0.5s / (1/60)) ≈ 30 апдейтов максимум.
        // ----------------------------------------------------------------------------------------
        const int maxUpdatesPerFrame =
            static_cast<int>(core::time::TimeService::kMaxAccumulatedSeconds /
                             fixedTimeStep.asSeconds());

        LOG_INFO(core::log::cat::Gameplay, "Game loop started");

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        std::uint64_t frameCount = 0;
#endif

        while (mWindow.isOpen()) {
            // Обновляем время кадра (raw dt, scaled dt, FPS/метрики).
            mTime.tick();

            // Обрабатываем события окна и ввода.
            processEvents();

            // ----------------------------------------------------------------------------------------
            // Выполняем один или несколько фиксированных шагов логики.
            // Cap на количество апдейтов за кадр предотвращает spiral of death при лагах.
            // ----------------------------------------------------------------------------------------
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
                mAircraftControlSystem->onKeyEvent(keyPressed->code, true);

                // Debug overlay toggle: хоткей работает во всех сборках (Debug/Release/Profile).
                // Политика: хоткей ВСЕГДА активен, независимо от дефолтного состояния overlay
                // при старте.
                if (keyPressed->code == dbg::HOTKEY_TOGGLE_OVERLAY) {
                    mDebugOverlay->setEnabled(!mDebugOverlay->isEnabled());
                }
            }
            // Отпускание клавиш.
            else if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>()) {
                mAircraftControlSystem->onKeyEvent(keyReleased->code, false);
            }

            // Потеря фокуса: сбрасываем ввод, чтобы не залипали клавиши при Alt-Tab/клике мимо окна.
            else if (event->is<sf::Event::FocusLost>()) {
                mAircraftControlSystem->resetState();
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