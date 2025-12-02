// ================================================================================================
// File: core/resources/paths/resource_paths.h
// Purpose: Maps resource enum IDs to file system paths via JSON registry.
// Used by: ResourceManager, higher-level loading code.
// Notes:
//  - Centralized, read-only lookup: ID -> filesystem path.
//  - Backed by internal static maps (Meyer's singleton) for safe initialization.
//  - Does NOT deal with hot-reload, mods, DLC, or virtual filesystems.
//    Those are implemented at higher layers.
// ================================================================================================
#pragma once

#include <string>
#include <unordered_map>

#include "core/resources/ids/resource_ids.h"

namespace core::resources::paths {

    /**
     * @brief Централизованное хранилище путей к ресурсам.
     *
     * Логика:
     *  - один раз загружается из JSON-файла (resources.json) при старте;
     *  - хранит соответствия enum ID -> строковый путь;
     *  - предоставляет быстрый и типобезопасный доступ к путям.
     *
     * Реализация:
     *  - внутри использует Meyer's Singleton для static map'ов, что даёт
     *    безопасную инициализацию в многопоточной среде;
     *  - интерфейс — статические методы (утилитарный "реестр").
     */
    class ResourcePaths {
      public:
        /**
         * @brief Загрузить реестр ресурсов из JSON-файла.
         *
         * @param filename Путь к JSON с описанием ресурсов (resources.json).
         *
         * Критические ошибки (нет файла, невалидный JSON, неверная структура)
         * приводят к выводу сообщения об ошибке и std::exit(EXIT_FAILURE),
         * т.к. без реестра ресурсов игра продолжать работу не может.
         */
        static void loadFromJSON(const std::string& filename);

        // --------------------------------------------------------------------
        // Получить путь к ресурсу (бросает std::runtime_error, если не найден)
        // --------------------------------------------------------------------

        [[nodiscard]] static const std::string& get(ids::TextureID id);
        [[nodiscard]] static const std::string& get(ids::FontID id);
        [[nodiscard]] static const std::string& get(ids::SoundID id);

        // --------------------------------------------------------------------
        // Проверить наличие ресурса (без исключений)
        // --------------------------------------------------------------------

        [[nodiscard]] static bool contains(ids::TextureID id) noexcept;
        [[nodiscard]] static bool contains(ids::FontID id) noexcept;
        [[nodiscard]] static bool contains(ids::SoundID id) noexcept;

        // --------------------------------------------------------------------
        // Получить все пути (для batch-загрузки / предзагрузки)
        // --------------------------------------------------------------------

        [[nodiscard]] static const std::unordered_map<ids::TextureID, std::string>&
        getAllTexturePaths() noexcept;

        [[nodiscard]] static const std::unordered_map<ids::FontID, std::string>&
        getAllFontPaths() noexcept;

        [[nodiscard]] static const std::unordered_map<ids::SoundID, std::string>&
        getAllSoundPaths() noexcept;

      private:
        // Meyer's Singleton для гарантии корректной инициализации
        static std::unordered_map<ids::TextureID, std::string>& getTextureMap();
        static std::unordered_map<ids::FontID, std::string>& getFontMap();
        static std::unordered_map<ids::SoundID, std::string>& getSoundMap();
    };

} // namespace core::resources::paths