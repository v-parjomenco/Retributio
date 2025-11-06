#include <cassert>
#include "core/game.h"
#include "core/config_loader.h"
#include "core/resources/resource_paths.h"

namespace core {

	Game::Game() :
		mWindow(sf::VideoMode({ config::WINDOW_WIDTH, config::WINDOW_HEIGHT }), config::WINDOW_TITLE) {

		// Загружаем ресурсы JSON
		core::resources::ResourcePaths::loadFromJSON("data/definitions/resources.json");

		// Применяем настройки рендеринга из config.h
		if constexpr (config::ENABLE_VSYNC) {
			mWindow.setVerticalSyncEnabled(true); // включаем вертикальную синхронизацию
			DEBUG_MSG("[Config] VSync enabled (FRAME_LIMIT ignored)");
		}
		else if constexpr (config::FRAME_LIMIT > 0) {
			mWindow.setFramerateLimit(config::FRAME_LIMIT); // ограничение FPS
		}

		// Инициализация шрифта по умолчанию для отображения FPS
		mTextOutput.init(mResources.getFont(resources::FontID::Default).get(), mText);

		initWorld(); // создаём ECS-мир и сущности
	}

	void Game::initWorld() {
		try {
			// Создаём конфигурацию игрока (из player.json)
			core::PlayerConfig playerCfg = core::ConfigLoader::loadPlayerConfig("assets/config/player.json");

			// Получаем текстуру по пути
			const sf::Texture& playerTexture =
				mResources.getTextureByPath(playerCfg.texturePath, true).get(); // smooth = true

			// Создаём сущность игрока
			mPlayerEntity = mWorld.createEntity();

			// Добавляем компонент с конфигурацией игрока (из player.json) в ECS-мир
			// PlayerInitSystem при первом апдейте создаст остальные компоненты (Sprite, Transform и т.д.)
			mWorld.addComponent(mPlayerEntity, core::ecs::PlayerConfigComponent{ playerCfg });

			// Подключаем ECS-системы

			mWorld.addSystem<core::ecs::PlayerInitSystem>(mResources);
			mWorld.addSystem<core::ecs::MovementSystem>();
			mWorld.addSystem<core::ecs::RenderSystem>();
			// Эти системы требуют прямого доступа (onResize, onKeyEvent), поэтому сохраняем указатели
			mScalingSystem = &mWorld.addSystem<core::ecs::ScalingSystem>();
			mLockSystem = &mWorld.addSystem<core::ecs::LockSystem>();
			mInputSystem = &mWorld.addSystem<core::ecs::InputSystem>();
		}
		catch (const std::exception& e) {
			// Кросс-платформенная обработка ошибок
#ifdef _WIN32
			utils::message::showError(std::string("Ошибка при инициализации ECS: ") + e.what());
			utils::message::holdOnExit();
#else
			std::cerr << "Ошибка при инициализации ECS: " << e.what() << std::endl;
#endif
			// fallback: создаём безопасный пустой мир
			mWorld = std::move(core::ecs::World());
		}
	}

	void Game::run() {
		assert(mWindow.isOpen()); // проверка, что окно открылось
		while (mWindow.isOpen()) {
			processEvents(); // для базовой логики и стабильных кадров
			mTime.update(); // обновили таймер и FPS

			// Если накопилось достаточно времени — выполняем апдейт
			while (mTime.shouldUpdate(config::FIXED_TIME_STEP)) {
				processEvents(); // обрабатываем события повторно, если фрэймрейт упал)
								 // это необязательно, но может помочь с отзывчивостью программы при подвисании
								 // окно всё равно будет реагировать на клавиши и не зависнет “в воздухе”.

				update(config::FIXED_TIME_STEP);
			}
			render(); // рендерим столько раз, сколько позволяет GPU
		}
	}

	void Game::processEvents() {

		while (const std::optional<sf::Event> event = mWindow.pollEvent()) {

			// обработка закрытия окна
			if (event->is<sf::Event::Closed>()) {
				mWindow.close();
			}

            // обработка нажатия и отпускания клавиш
			else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
				if (mInputSystem) { mInputSystem->onKeyEvent(keyPressed->code, true); 
				}
			}
			else if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>()) {
				if (mInputSystem) {
					mInputSystem->onKeyEvent(keyReleased->code, false);
				}
			}

            // обработка изменения размера окна
				else if (const auto* resized = event->getIf<sf::Event::Resized>()) {
					sf::Vector2f newSize(static_cast<float>(resized->size.x), static_cast<float>(resized->size.y));
					sf::View newView({ newSize.x / 2.f, newSize.y / 2.f }, { newSize.x, newSize.y });
					// Обновляем View окна, чтобы другие поведения и отрисовка видели актуальный вид
					mWindow.setView(newView);
					if (mScalingSystem) mScalingSystem->onResize(mWorld, newView); // безопасный вызов (null-check)
					if (mLockSystem)    mLockSystem->onResize(mWorld, newView);	   // безопасный вызов (null-check)
				}
		}
	}

	void Game::update(const sf::Time& dt) {

		assert(dt.asSeconds() > 0); // время обновления должно быть положительным		
		mTextOutput.updateFpsText(mText, mTime.getSmoothedFps());// выводим сглаженный FPS

		mWorld.update(dt.asSeconds()); // обновляем все ECS-системы
	}

	void Game::render() {
		mWindow.clear();
		mWorld.render(mWindow); // отрисовываем ECS-мир
		mTextOutput.draw(mWindow, mText);
		mWindow.display();
	}
} // namespace core