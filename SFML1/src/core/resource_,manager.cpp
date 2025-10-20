#include <cassert>
#include "core/resource_manager.h"

namespace core {

	// Загружаем шрифт и проверяем корректность загрузки
	const sf::Font& ResourceManager::getFont(const std::string& filename) {

		// Проверка, чтобы избежать повторной загрузки одного и того же шрифта
		static bool fontLoaded = false;
		assert(!fontLoaded && "Шрифт уже был загружен ранее!");

		if (!mFont.openFromFile(filename)) {
			// Не удалось загрузить шрифт
			throw std::runtime_error("Не удалось загрузить шрифт: " + filename);
		}

		fontLoaded = true; // отмечаем, что шрифт загружен
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

		// Если вдруг будем вставлять прямо этим методом, на будущее
		//auto inserted = mTextures.emplace(filename, std::move(texture));
		//Здесь assert проверяет, что ключ уникален
		//assert(inserted.second);


		return mTextures.emplace(filename, std::move(texture)).first->second;
	}

} // namespace core