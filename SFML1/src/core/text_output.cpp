#include "core/text_output.h"

namespace core {

	void TextOutput::init(const sf::Font& font, std::optional<sf::Text>& text) const {

		text.emplace(font); // инициализация объекта std::optional   
		text->setCharacterSize(35); // установка размера текста  по умолчанию
		text->setFillColor(sf::Color::Red); // установка цвета текста красным  по умолчанию
		text->setPosition({ 10.f, 10.f }); // установка позиции текста по умолчанию
	}

	void TextOutput::updateFpsText(std::optional<sf::Text>& text, float fps) const {
		if (text)
			text->setString("FPS: " + std::to_string(static_cast<int>(fps)));
	}

	void TextOutput::draw(sf::RenderWindow& window, std::optional<sf::Text>& text) const {
		if (text) {
			window.draw(*text);
		}
	}	
} // namespace core