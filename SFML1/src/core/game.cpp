#include "core/game.h"
#include "core/config_loader.h"

namespace core {

	Game::Game() :
		mWindow(sf::VideoMode({ config::WINDOW_WIDTH, config::WINDOW_HEIGHT }), config::WINDOW_TITLE) {

		// Применяем настройки рендеринга из config.h
		if (config::ENABLE_VSYNC) {
			mWindow.setVerticalSyncEnabled(true); // включаем вертикальную синхронизацию
			DEBUG_MSG("[Config] VSync включён (FRAME_LIMIT игнорируется)");
		}
		else if (config::FRAME_LIMIT > 0) {
			mWindow.setFramerateLimit(config::FRAME_LIMIT); // ограничение FPS
		}

		// Инициализация текста FPS
		mTextOutput.init(mResourceManager.getFont("assets/fonts/Wolgadeutsche.otf"), mText);

		// Загружаем конфигурацию игрока
		try {
			core::PlayerConfig playerConfig = core::ConfigLoader::loadPlayerConfig("assets/config/player.json");
			const sf::Texture& playerTexture = mResourceManager.getTexture(playerConfig.texture);
			mPlayer = std::make_unique<entities::Player>(playerTexture);
            mPlayer->initFromConfig(playerConfig); // инициализируем игрока из конфигурации
		}
		catch (const std::exception& e) {
			// Кросс-платформенная обработка ошибок
#ifdef _WIN32
            utils::message::showError(std::string("Ошибка при загрузке игрока: ") + (e.what()));
			utils::message::holdOnExit();
#else
            std::cerr << "Ошибка при загрузке игрока. " << e.what() << std::endl;
#endif
			// fallback создаём игрока с настройками по умолчанию
			const sf::Texture& playerTexture = mResourceManager.getTexture(config::PLAYER_TEXTURE);
			mPlayer = std::make_unique<entities::Player>(playerTexture);
		}
	}

	void Game::run() {
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
				handlePlayerInput(keyPressed->code, true);
			}
			else if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>()) {
				handlePlayerInput(keyReleased->code, false);
			}

            // обработка изменения размера окна
				else if (const auto* resized = event->getIf<sf::Event::Resized>()) {
					sf::Vector2f newSize(static_cast<float>(resized->size.x), static_cast<float>(resized->size.y));
					sf::View newView({ newSize.x / 2.f, newSize.y / 2.f }, { newSize.x, newSize.y });
					// Обновляем View окна, чтобы другие поведения и отрисовка видели актуальный вид
					mWindow.setView(newView);
					// Игрок (а потом добавить и остальные объекты)
					mPlayer->onResize(newView);
				}
		}
	}

	void Game::handlePlayerInput(sf::Keyboard::Key key, bool isPressed) {
		mPlayer->handleInput(key, isPressed);
	}

	void Game::update(const sf::Time& dt) {
		
		mTextOutput.updateFpsText(mText, mTime.getSmoothedFps());// выводим сглаженный FPS
		mPlayer->update(dt); // обновляем игрока
	}

	void Game::render() {
		mWindow.clear();
		mPlayer->draw(mWindow);
		mTextOutput.draw(mWindow, mText);
		mWindow.display();
	}
} // namespace core