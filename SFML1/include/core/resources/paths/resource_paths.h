// ================================================================================================
// File: core/resources/paths/resource_paths.h
// Purpose: Maps resource enum IDs (TextureID / FontID / SoundID) to file system paths
// Used by: ResourceManager (static resources known at engine start)
// Related headers: resource_ids.h, resource_id_utils.h, config_keys.h
// ================================================================================================
#pragma once

#include <string>
#include <unordered_map>

#include "core/resources/ids/resource_ids.h"

namespace core::resources::paths {

    /**
    * @brief Централизованное хранилище путей к ресурсам по их enum-идентификаторам.
    * Использует assets/game/skyguard/config/resources.json:
    * {
    *   "textures": { "Player": "assets/game/skyguard/images/su-57.png", ... },
    *   "fonts":    { "Default": "assets/core/fonts/Wolgadeutsche.otf", ... },
    *   "sounds":   { ... }
    * }
    * Загрузка выполняется один раз при старте игры (или сцены).
    */
    class ResourcePaths {
      public:
        /**
        * @brief Загружает реестр ресурсов из JSON-файла.
        * @param filename Путь к JSON (обычно assets/game/skyguard/config/resources.json)
        *
        * При критических ошибках формата/чтения:
        *  - выводит понятное сообщение пользователю;
        *  - завершает приложение (fail-fast).
        */
        static void loadFromJSON(const std::string& filename);

        // Возвращает путь к текстуре по enum-идентификатору.
        static const std::string& get(ids::TextureID id);

        // Возвращает путь к шрифту по enum-идентификатору.
        static const std::string& get(ids::FontID id);

        // Возвращает путь к звуковому буферу по enum-идентификатору.
        static const std::string& get(ids::SoundID id);

      private:
        // Внутренние таблицы соответствий ID → path.

        static std::unordered_map<ids::TextureID, std::string> mTextures;
        static std::unordered_map<ids::FontID, std::string> mFonts;
        static std::unordered_map<ids::SoundID, std::string> mSounds;
    };

} // namespace core::resources::paths