#pragma once

#include <string>
#include <type_traits>
#include <SFML/Graphics.hpp>
#include "core/i_resource.h"

namespace core::resources {

    // TextureResource — thin wrapper вокруг sf::Texture.
    // Делает uniform-интерфейс для ResourceHolder::load(...).
    class TextureResource : public core::IResource {
    public:
        TextureResource() = default;
        ~TextureResource() override = default;

        // Базовый метод
        bool loadFromFile(const std::string& filename) override {
            if (!mTexture.loadFromFile(filename)) return false;
            // НЕ устанавливаем сглаживание здесь "вслепую".
            // mSmooth содержит предпочтение (по умолчанию true), но реальная установка
            // будет выполняться контролируемо в ResourceManager/ResourceLoader через setSmooth().
            return true;
        }

        // Вариативная версия для будущих аргументов SFML 3
        // Может поддерживать IntRect, sRGB, флаг сглаживания и т.д.
        template <typename... Args>
        bool loadFromFile(const std::string& filename, Args&&... args) {
            if constexpr (sizeof...(Args) == 0) {
                return loadFromFile(filename); // базовая версия
            }
            else {
                if constexpr (std::is_invocable_v<decltype(&sf::Texture::loadFromFile), sf::Texture*, const std::string&, Args...>) {
                    if (!mTexture.loadFromFile(filename, std::forward<Args>(args)...)) return false;
                    mTexture.setSmooth(mSmooth);
                    return true;
                }
                else {
                    // sf::Texture не поддерживает аргументы → базовая версия
                    return loadFromFile(filename);
                }
            }
        }

        // Явное управление сглаживанием, если нужно отключить/включить
        void setSmooth(bool smooth) { mTexture.setSmooth(smooth); }
        bool isSmooth() const { return mTexture.isSmooth(); }

        sf::Texture& get() { return mTexture; }
        const sf::Texture& get() const { return mTexture; }

    private:
        sf::Texture mTexture;
        bool mSmooth = true; // по умолчанию сглаживание включено
    };

} // namespace core::resources