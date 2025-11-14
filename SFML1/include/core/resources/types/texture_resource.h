#pragma once

#include <string>
#include <utility>
#include <SFML/Graphics.hpp>
#include "core/resources/i_resource.h"

namespace core::resources::types {

    // TextureResource — thin wrapper вокруг sf::Texture.
    // Делает uniform-интерфейс для ResourceHolder::load(...).
    class TextureResource : public core::resources::IResource {
    public:
        TextureResource() = default;
        ~TextureResource() override = default;

        // Базовый метод, требуемый интерфейсом IResource.
        // Минимальная гарантия: пытаемся загрузить текстуру из файла.
        bool loadFromFile(const std::string& filename) override {
            return mTexture.loadFromFile(filename);
        }

        // Вариативная версия для будущих аргументов SFML 3
        // Может поддерживать IntRect, sRGB, флаг сглаживания и т.д.
        // Если аргументов нет — просто зовём базовую версию.
        template <typename... Args>
        bool loadFromFile(const std::string& filename, Args&&... args) {
            if constexpr (sizeof...(Args) == 0) {
                return loadFromFile(filename); // базовая версия
            }
            else {
                return mTexture.loadFromFile(filename, std::forward<Args>(args)...);
            }
        }

        // Явное управление сглаживанием, если нужно отключить/включить
        void setSmooth(bool smooth) { mTexture.setSmooth(smooth); }
        bool isSmooth() const { return mTexture.isSmooth(); }

        // Доступ к сырой sf::Texture
        sf::Texture& get() { return mTexture; }
        const sf::Texture& get() const { return mTexture; }

    private:
        sf::Texture mTexture;
        bool mSmooth = true; // по умолчанию сглаживание включено
    };

} // namespace core::resources::types