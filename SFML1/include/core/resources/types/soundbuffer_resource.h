// ================================================================================================
// File: core/resources/types/soundbuffer_resource.h
// Purpose: Thin wrapper around sf::SoundBuffer for resource management.
// Used by: ResourceHolder, ResourceManager, ResourceLoader.
// Notes:
//  - Move-only обёртка над sf::SoundBuffer.
//  - Содержит базовые метрики (кол-во сэмплов, примерный размер в памяти).
// ================================================================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include <SFML/Audio/SoundBuffer.hpp>

namespace core::resources::types {

    /**
     * @brief SoundBufferResource — тонкая обёртка над sf::SoundBuffer.
     *
     * Обязанности:
     *  - предоставить loadFromFile(...) для ResourceHolder / ResourceLoader;
     *  - инкапсулировать sf::SoundBuffer с запретом копирования;
     *  - дать базовые метрики (количество сэмплов, примерный объём памяти).
     */
    class SoundBufferResource {
      public:
        SoundBufferResource() = default;
        ~SoundBufferResource() = default;

        // Запрещаем копирование (аудиобуферы тяжёлые).
        SoundBufferResource(const SoundBufferResource&) = delete;
        SoundBufferResource& operator=(const SoundBufferResource&) = delete;

        // Разрешаем перемещение.
        SoundBufferResource(SoundBufferResource&&) noexcept = default;
        SoundBufferResource& operator=(SoundBufferResource&&) noexcept = default;

        /// @brief Базовая загрузка звукового буфера из файла.
        [[nodiscard]] bool loadFromFile(const std::string& filename) {
            return mBuffer.loadFromFile(filename);
        }

        /// @brief Доступ к сырому sf::SoundBuffer.
        [[nodiscard]] sf::SoundBuffer& get() noexcept {
            return mBuffer;
        }

        [[nodiscard]] const sf::SoundBuffer& get() const noexcept {
            return mBuffer;
        }

        /// @brief Количество сэмплов в буфере (включая все каналы).
        [[nodiscard]] std::size_t getSampleCount() const noexcept {
            return mBuffer.getSampleCount();
        }

        /// @brief Приблизительный размер в байтах (в основном буфере).
        ///
        /// SFML 3 хранит аудио как массив 16-битных сэмплов (std::int16_t),
        /// getSampleCount() возвращает общее количество сэмплов по всем каналам.
        [[nodiscard]] std::size_t getMemorySize() const noexcept {
            return mBuffer.getSampleCount() * sizeof(std::int16_t);
        }

      private:
        sf::SoundBuffer mBuffer;
    };

} // namespace core::resources::types
