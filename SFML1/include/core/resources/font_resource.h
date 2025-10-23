#pragma once

#include <string>
#include <SFML/Graphics.hpp>
#include "core/i_resource.h"

namespace core::resources {

    // FontResource — тонкая оболочка вокруг sf::Font
    class FontResource : public core::IResource {
    public:
        FontResource() = default;
        ~FontResource() override = default;

        // Базовый метод
        bool loadFromFile(const std::string& filename) override {
            return mFont.openFromFile(filename);
        }

        sf::Font& get() { return mFont; }
        const sf::Font& get() const { return mFont; }

    private:
        sf::Font mFont;
    };

} // namespace core::resources