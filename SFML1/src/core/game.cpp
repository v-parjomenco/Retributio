#include "pch.h"

#include "core/game.h"
#include <cassert>
#include <iostream>

#include "core/config/loader/debug_overlay_loader.h"
#include "core/resources/paths/resource_paths.h"
#include "core/utils/message.h"

#include "game/skyguard/config/loader/config_loader.h"
#include "game/skyguard/ecs/components/player_config_component.h"

namespace cfg = ::config;        // глобальные дефолты движка (окно, vsync, fixed step, hotkeys...)
namespace gcfg = ::core::config; // core::config — для debug overlay и других движковых конфигов
// game-specific конфиги/blueprints для Skyguard
namespace skycfg = ::game::skyguard::config;

namespace core {

    Game::Game()
        : mWindow(sf::VideoMode({cfg::WINDOW_WIDTH, cfg::WINDOW_HEIGHT}), cfg::WINDOW_TITLE) {

        // Загружаем ресурсы/пути из JSON
        core::resources::paths::ResourcePaths::loadFromJSON(
            "assets/game/skyguard/config/resources.json");

        // Применяем настройки рендеринга из config.h
        if constexpr (cfg::ENABLE_VSYNC) {
            mWindow.setVerticalSyncEnabled(true); // включаем вертикальную синхронизацию
            DEBUG_MSG("[Game]\nVSync enabled (FRAME_LIMIT ignored)");
        } else if constexpr (cfg::FRAME_LIMIT > 0) {
            mWindow.setFramerateLimit(cfg::FRAME_LIMIT); // ограничение FPS
        }

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

            // Подключаем ECS-системы

            mWorld.addSystem<game::skyguard::ecs::PlayerInitSystem>(mResources);
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
                const sf::Font& font = mResources.getFont(resources::ids::FontID::Default).get();
                mDebugOverlay->bind(mTime, font);

                // Грузим конфиг для DebugOverlay
                const auto overlayCfg =
                    gcfg::loadDebugOverlayBlueprint("assets/core/config/debug_overlay.json");

                // Применяем стиль
                mDebugOverlay->applyTextProperties(overlayCfg.text);

                // enabled = config.json ∧ дефолт из config.h (Debug=true, Release=false)
                mDebugOverlay->setEnabled(overlayCfg.enabled && cfg::SHOW_FPS_OVERLAY);
            }
        } catch (const std::exception& e) {
            // Кросс-платформенная обработка ошибок
#ifdef _WIN32
            utils::message::showError(std::string("Ошибка при инициализации ECS: ") + e.what());
            utils::message::holdOnExit();
#else
            std::cerr << "Ошибка при инициализации ECS: " << e.what() << std::endl;
#endif
            mWorld = std::move(core::ecs::World()); // fallback: создаём безопасный пустой мир
        }
    }

    void Game::run() {
        assert(mWindow.isOpen()); // проверка, что окно открылось
        while (mWindow.isOpen()) {
            processEvents(); // для базовой логики и стабильных кадров
            mTime.tick();    // обновляем время ровно 1 раз на кадр

            // Если накопилось достаточно времени — выполняем апдейт
            while (mTime.shouldUpdate(cfg::FIXED_TIME_STEP)) {
                processEvents(); // обрабатываем события повторно, если фрэймрейт упал)
                // это необязательно, но может помочь с отзывчивостью программы при подвисании
                // окно всё равно будет реагировать на клавиши и не зависнет “в воздухе”.

                update(cfg::FIXED_TIME_STEP);
            }
            render(); // рендерим столько раз, сколько позволяет GPU
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

    void Game::update(const sf::Time& dt) {
        assert(dt.asSeconds() > 0);    // время обновления должно быть положительным
        mWorld.update(dt.asSeconds()); // обновляем все ECS-системы
    }

    void Game::render() {
        mWindow.clear();
        mWorld.render(mWindow); // отрисовываем ECS-мир
        mWindow.display();
    }
} // namespace core