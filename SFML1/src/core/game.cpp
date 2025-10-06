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
			core::PlayerConfig pconf = core::ConfigLoader::loadPlayerConfig("assets/config/player.json");
			const sf::Texture& playerTexture = mResourceManager.getTexture(pconf.texture);
			mPlayer = std::make_unique<entities::Player>(playerTexture);
            mPlayer->initFromConfig(pconf); // инициализируем игрока из конфигурации
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

			if (event->is<sf::Event::Closed>()) {
				// обработка закрытия окна
				mWindow.close();
			}
			else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
				// обработка нажатия клавиши
				handlePlayerInput(keyPressed->code, true);
			}
			else if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>()) {
				// обработка отпускания клавиши
				handlePlayerInput(keyReleased->code, false);
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