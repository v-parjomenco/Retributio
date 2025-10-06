#pragma once

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <SFML/Graphics.hpp>

namespace core {

    class ResourceManager {
    public:

        // Загружаем шрифт и проверяем корректность загрузки
        const sf::Font& getFont(const std::string& filename);

        // Загрузка текстуры с кешированием
        const sf::Texture& getTexture(const std::string& filename);

    private:
        sf::Font mFont;
        std::unordered_map<std::string, sf::Texture> mTextures;
    };

} // namespace core
