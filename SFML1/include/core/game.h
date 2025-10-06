#pragma once

#include <SFML/Graphics.hpp>
#include "core/config.h"
#include "core/text_output.h"
#include "core/time_system.h"
#include "core/resource_manager.h"
#include "entities/player.h"
#include "utils/message.h"

namespace core {

	class Game {
	public:
		Game();
		void run();
	private:
		void processEvents();
		void update(const sf::Time& dt);
		void render();
		void handlePlayerInput(sf::Keyboard::Key key, bool isPressed);
	private:
		sf::RenderWindow mWindow;
		core::ResourceManager mResourceManager;
		core::TimeSystem mTime; // система для отслеживания времени
		core::TextOutput mTextOutput; // система для вывода текста
		std::optional<sf::Text> mText;
		std::unique_ptr<entities::Player> mPlayer; // Используем unique_ptr для управления временем жизни игрока
											   //  unique_ptr позволяет отложить создание (или пересоздание),
											   //  инициализировать позже
	};

} // namespace core