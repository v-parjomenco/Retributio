// ================================================================================================
// File: core/resources/types/font_resource.h
// Purpose: Thin wrapper around sf::Font for resource management.
// Used by: ResourceHolder, ResourceManager, ResourceLoader.
// Notes:
//  - Move-only обёртка над sf::Font без виртуального базового класса.
//  - Сфокусирована только на загрузке и доступе к шрифту.
// ================================================================================================
#pragma once

#include <string>

#include <SFML/Graphics/Font.hpp>

namespace core::resources::types {

    /**
     * @brief FontResource — тонкая обёртка над sf::Font.
     *
     * Обязанности:
     *  - предоставить loadFromFile(...) для ResourceHolder / ResourceLoader;
     *  - инкапсулировать sf::Font c запретом копирования;
     *  - дать прямой доступ к sf::Font для текста/GUI.
     *
     * Детальные метрики по памяти для шрифтов зависят от внутренних текстур
     * и кэшей glyph'ов и требуют отдельной системы — поэтому здесь не считаются.
     */
    class FontResource {
      public:
        FontResource() = default;
        ~FontResource() = default;

        // Запрещаем копирование (шрифты тяжёлые, держатся в кэше и текстурах).
        FontResource(const FontResource&) = delete;
        FontResource& operator=(const FontResource&) = delete;

        // Разрешаем перемещение.
        FontResource(FontResource&&) noexcept = default;
        FontResource& operator=(FontResource&&) noexcept = default;

        /// @brief Базовая загрузка шрифта из файла.
        [[nodiscard]] bool loadFromFile(const std::string& filename) {
            return mFont.openFromFile(filename);
        }

        /// @brief Доступ к сырому sf::Font.
        [[nodiscard]] sf::Font& get() noexcept {
            return mFont;
        }

        [[nodiscard]] const sf::Font& get() const noexcept {
            return mFont;
        }

      private:
        sf::Font mFont;
    };

} // namespace core::resources::types