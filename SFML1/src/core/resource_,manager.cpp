#include "core/resource_manager.h"

namespace core {

	// Загружаем шрифт и проверяем корректность загрузки
	const sf::Font& ResourceManager::getFont(const std::string& filename) {
		if (!mFont.openFromFile(filename)) {
			// Не удалось загрузить шрифт
			throw std::runtime_error("Не удалось загрузить шрифт: " + filename);
		}
		return mFont;
	}

	// Загрузка текстуры с кешированием       
	const sf::Texture& ResourceManager::getTexture(const std::string& filename) {
		auto it = mTextures.find(filename);
		if (it != mTextures.end()) return it->second;

		sf::Texture texture;
		if (!texture.loadFromFile(filename))
			throw std::runtime_error("Не удалось загрузить текстуру: " + filename);
		texture.setSmooth(true); // Включаем сглаживание (антиалиасинг, если отключено)
		return mTextures.emplace(filename, std::move(texture)).first->second;
	}

} // namespace core