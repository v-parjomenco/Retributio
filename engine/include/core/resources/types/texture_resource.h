// ================================================================================================
// File: core/resources/types/texture_resource.h
// Purpose: Thin wrapper around sf::Texture with a uniform loadFromFile interface
//          and basic 4X-friendly features (mipmaps, tiling, memory metrics).
// Used by: ResourceManager.
// Notes:
//  - Does NOT own any higher-level policy (hot-reload, streaming, etc.).
//    Those are implemented at ResourceManager / tools level.
//  - Designed as a lightweight value-type wrapper around sf::Texture.
// ================================================================================================
#pragma once

#include <cstddef>
#include <filesystem>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/Texture.hpp>

namespace core::resources::types {

    /**
     * @brief TextureResource — тонкая обёртка над sf::Texture.
     *
     * Обязанности:
     *  - предоставить единый интерфейс loadFromFile(...) для ResourceManager;
     *  - инкапсулировать тяжёлый ресурс (sf::Texture) с запретом копирования;
     *  - добавить базовые фичи, полезные для 4X (mipmap, tiling, профилирование размера).
     *
     * НЕ занимается:
     *  - горячей перезагрузкой, слежением за файлом, профилями UI/terrain и т.п.;
     *  - асинхронной загрузкой, стримингом, компрессией.
     *
     * Всё это — ответственность уровнем выше (ResourceManager / тулзы).
     */
    class TextureResource {
      public:
        TextureResource() = default;
        ~TextureResource() = default;

        // ----------------------------------------------------------------------------------------
        // Семантика владения
        // ----------------------------------------------------------------------------------------
        // Текстуры тяжёлые, копирование запрещено
        TextureResource(const TextureResource&) = delete;
        TextureResource& operator=(const TextureResource&) = delete;

        // Разрешаем перемещение.
        TextureResource(TextureResource&&) noexcept = default;
        TextureResource& operator=(TextureResource&&) noexcept = default;

        // ----------------------------------------------------------------------------------------
        // Загрузка из файла (под сигнатуры SFML 3.0.2).
        // ----------------------------------------------------------------------------------------

        /// @brief Базовая загрузка всей текстуры из файла.
        ///
        /// Соответствует вызову sf::Texture::loadFromFile(path)
        /// без sRGB и без выделенного прямоугольника.
        [[nodiscard]] bool loadFromFile(const std::filesystem::path& filename) {
            return mTexture.loadFromFile(filename);
        }

        /// @brief Загрузка всей текстуры из файла с управлением sRGB.
        ///
        /// Эквивалентно sf::Texture::loadFromFile(path, sRgb, area) с area = {}.
        [[nodiscard]] bool loadFromFile(const std::filesystem::path& filename, bool sRgb) {
            return loadFromFile(filename, sRgb, sf::IntRect{});
        }

        /// @brief Загрузка части текстуры с управлением sRGB.
        ///
        /// Соответствует sf::Texture::loadFromFile(path, sRgb, area) в SFML 3.
        /// Полезно для:
        ///  - texture atlas / sprite sheet (area),
        ///  - управления цветовым пространством (sRgb).
        [[nodiscard]] bool loadFromFile(const std::filesystem::path& filename, 
                                        bool sRgb,
                                        const sf::IntRect& area) {
            return mTexture.loadFromFile(filename, sRgb, area);
        }

        // ----------------------------------------------------------------------------------------
        // Настройки отображения (качество / тайлинг).
        // ----------------------------------------------------------------------------------------

        /// @brief Включить/выключить сглаживание (для UI обычно true, для пиксель-арта false).
        void setSmooth(bool smooth) noexcept {
            mTexture.setSmooth(smooth);
        }

        [[nodiscard]] bool isSmooth() const noexcept {
            return mTexture.isSmooth();
        }

        /// @brief Включить режим повтора (tiling) — критично для тайловых карт/фонов.
        void setRepeated(bool repeated) noexcept {
            mTexture.setRepeated(repeated);
        }

        [[nodiscard]] bool isRepeated() const noexcept {
            return mTexture.isRepeated();
        }

        /// @brief Генерация мипмапов (LOD для сильного отдаления камеры в 4X).
        /// Вызывать ПОСЛЕ успешного loadFromFile(...).
        [[nodiscard]] bool generateMipmap() {
            return mTexture.generateMipmap();
        }

        // ----------------------------------------------------------------------------------------
        // Доступ к sf::Texture и базовые метрики.
        // ----------------------------------------------------------------------------------------

        /// @brief Доступ к сырой sf::Texture (для интеграции с SFML-рендером).
        [[nodiscard]] sf::Texture& get() noexcept {
            return mTexture;
        }

        [[nodiscard]] const sf::Texture& get() const noexcept {
            return mTexture;
        }

        /// @brief Размер текстуры в пикселях.
        [[nodiscard]] sf::Vector2u getSize() const noexcept {
            return mTexture.getSize();
        }

        /// @brief Приблизительный размер в байтах в памяти 
        /// (без учёта драйверных нюансов).
        ///
        /// Предполагаем формат RGBA8 (4 байта на пиксель).
        /// Для rough-профилирования этого достаточно.
        [[nodiscard]] std::size_t getMemorySize() const noexcept {
            const auto size = mTexture.getSize();
            return static_cast<std::size_t>(size.x) * size.y * 4u;
        }

      private:
        sf::Texture mTexture;
    };

} // namespace core::resources::types