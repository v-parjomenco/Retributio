#pragma once

#include <optional>
#include <SFML/Graphics.hpp>

namespace core {

	// Постепенно, когда проект растёт, из TextOutput вырастают 2 логичных системы:

	//1. UIManager(или HudSystem)

	//	Класс, который управляет всеми текстами, иконками, панелями интерфейса.
	//	Он хранит список всех sf::Text элементов и знает, как их обновлять и рисовать.

	//2. DebugOverlay(или DebugInfo)

	//	Используется только в отладочных сборках(#ifdef DEBUG) — показывает FPS, время апдейта, координаты игрока и т.п.
	//	Можно подключать / выключать по хоткею.


	class TextOutput {
	public:
		void init(const sf::Font& font, std::optional<sf::Text>& text) const;

		void updateFpsText(std::optional<sf::Text>& text, float fps) const;

		void draw(sf::RenderWindow& window, std::optional<sf::Text>& text) const;
	};
} // namespace core