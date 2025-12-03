// ================================================================================================
// File: core/resources/paths/resource_paths.h
// Purpose: Maps resource enum IDs to file system paths via JSON registry.
// Used by: ResourceManager, higher-level loading code.
// Notes:
//  - Centralized, read-only lookup: ID -> filesystem path (+ low-level config).
//  - Backed by internal static maps (Meyer's singleton) for safe initialization.
//  - Does NOT deal with hot-reload, mods, DLC, or virtual filesystems.
//    Those are implemented at higher layers.
// ================================================================================================
#pragma once

#include <string>
#include <unordered_map>

#include "core/resources/config/font_resource_config.h"
#include "core/resources/config/sound_resource_config.h"
#include "core/resources/config/texture_resource_config.h"
#include "core/resources/ids/resource_ids.h"

namespace core::resources::paths {

    /**
     * @brief Централизованное хранилище путей и базовой конфигурации ресурсов.
     *
     * Логика:
     *  - один раз загружается из JSON-файла (resources.json) при старте;
     *  - хранит соответствия enum ID -> конфиг ресурса (путь + флаги загрузки);
     *  - предоставляет быстрый и типобезопасный доступ к этим данным.
     *
     * Реализация:
     *  - внутри использует Meyer's Singleton для static map'ов, что даёт
     *    безопасную инициализацию в многопоточной среде;
     *  - интерфейс — статические методы (утилитарный "реестр").
     */
    class ResourcePaths {
      public:
        // Удобные псевдонимы для конфигураций.
        using TextureConfig = core::resources::config::TextureResourceConfig;
        using FontConfig = core::resources::config::FontResourceConfig;
        using SoundConfig = core::resources::config::SoundResourceConfig;

        /**
         * @brief Загрузить реестр ресурсов из JSON-файла.
         *
         * @param filename Путь к JSON с описанием ресурсов (resources.json).
         *
         * Критические ошибки (нет файла, невалидный JSON, неверная структура) приводят к выводу
         * сообщения об ошибке и std::exit(EXIT_FAILURE), т.к. без реестра ресурсов игра продолжать
         * работу не может.
         */
        static void loadFromJSON(const std::string& filename);

        // ----------------------------------------------------------------------------------------
        // Получить конфигурацию ресурса (бросает std::runtime_error, если она не найдена)
        // ----------------------------------------------------------------------------------------

        /**
         * @brief Получить полную конфигурацию текстурного ресурса.
         *
         * Если TextureID неизвестен — бросает std::runtime_error с сообщением в стиле
         * "[ResourcePaths::getTextureConfig(TextureID)] ...".
         *
         * Возвращает константную ссылку на внутренний реестр (read-only).
         * Мутировать возвращаемую конфигурацию категорически не рекомендуется.
         */
        [[nodiscard]] static const TextureConfig& getTextureConfig(ids::TextureID id);

        /**
         * @brief Получить полную конфигурацию шрифта.
         *
         * На текущем этапе FontConfig содержит только путь (path).
         * Позже сюда могут добавиться другие низкоуровневые параметры.
         */
        [[nodiscard]] static const FontConfig& getFontConfig(ids::FontID id);

        /**
         * @brief Получить полную конфигурацию звукового ресурса (SoundBuffer).
         *
         * На текущем этапе SoundConfig содержит только путь (path).
         * Позже сюда могут добавиться другие низкоуровневые параметры.
         */
        [[nodiscard]] static const SoundConfig& getSoundConfig(ids::SoundID id);

        // ----------------------------------------------------------------------------------------
        // Проверить наличие ресурса (без исключений)
        // ----------------------------------------------------------------------------------------

        [[nodiscard]] static bool contains(ids::TextureID id) noexcept;
        [[nodiscard]] static bool contains(ids::FontID id) noexcept;
        [[nodiscard]] static bool contains(ids::SoundID id) noexcept;

        // ----------------------------------------------------------------------------------------
        // Получить все ресурсы (для batch-загрузки / предзагрузки)
        // ----------------------------------------------------------------------------------------

        /**
         * @brief Доступ ко всем конфигурациям текстур.
         *
         * Используется, например, для глобальной предзагрузки ассетов
         * (preloadAllTextures в ResourceManager) или тулов.
         */
        [[nodiscard]] static const std::unordered_map<ids::TextureID, TextureConfig>&
        getAllTextureConfigs() noexcept;

        /**
         * @brief Доступ ко всем конфигурациям шрифтов.
         */
        [[nodiscard]] static const std::unordered_map<ids::FontID, FontConfig>&
        getAllFontConfigs() noexcept;

        /**
         * @brief Доступ ко всем конфигурациям звуков.
         */
        [[nodiscard]] static const std::unordered_map<ids::SoundID, SoundConfig>&
        getAllSoundConfigs() noexcept;

      private:
        // Meyer's Singleton для гарантии корректной инициализации
        static std::unordered_map<ids::TextureID, TextureConfig>& getTextureMap();
        static std::unordered_map<ids::FontID, FontConfig>& getFontMap();
        static std::unordered_map<ids::SoundID, SoundConfig>& getSoundMap();
    };

} // namespace core::resources::paths
