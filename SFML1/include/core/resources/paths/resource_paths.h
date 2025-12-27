// ================================================================================================
// File: core/resources/paths/resource_paths.h
// Purpose: Maps resource enum IDs to file system paths via JSON registry.
// Used by: ResourceManager, higher-level loading code.
//
// IMPORTANT: INTERNAL DETAIL OF THE RESOURCE LAYER
//  - ResourcePaths is an implementation detail and MUST NOT be accessed directly
//    by code outside ResourceManager and the composition root (bootstrap).
//  - All external code must use ResourceManager as the facade.
//  - Future: will be replaced by instance-owned ResourceRegistry when hot-reload,
//    mods, or multi-context scenarios are needed.
//
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
     *  - внутри использует Meyer's Singleton для static map'ов (C++11+ thread-safe init);
     *  - интерфейс — статические методы (утилитарный "реестр").
     *
     * ВАЖНО:
     *  - Это внутренность resource-layer. Снаружи только ResourceManager.
     */
    class ResourcePaths {
      public:
        using TextureConfig = core::resources::config::TextureResourceConfig;
        using FontConfig = core::resources::config::FontResourceConfig;
        using SoundConfig = core::resources::config::SoundResourceConfig;

        /**
         * @brief Загрузить реестр ресурсов из JSON-файла.
         *
         * @param filename Путь к JSON с описанием ресурсов (resources.json).
         *
         * Контракт:
         *  - Вызывается ровно один раз при старте приложения (до любых get* вызовов).
         *  - Повторная загрузка является programmer error (Debug: assert).
         *  - Критические ошибки (файл/JSON/структура) → LOG_PANIC.
         *  - textures/fonts: любая ошибка записи (тип/структура/ID) → LOG_PANIC.
         *  - sounds: отсутствующий блок допустим, битые записи → LOG_WARN и пропуск.
         */
        static void loadFromJSON(const std::string& filename);

        /**
         * @brief Получить полную конфигурацию текстурного ресурса.
         *
         * @param id Идентификатор текстуры (должен быть зарегистрирован в resources.json).
         *
         * Контракт:
         *  - ID должен существовать в реестре (проверяется через contains()
         *    или гарантируется инвариантами инициализации/кодогеном).
         *  - Если ID отсутствует → LOG_PANIC.
         *  - Возвращает константную ссылку (read-only).
         */
        [[nodiscard]] static const TextureConfig& getTextureConfig(ids::TextureID id);

        /**
         * @brief Получить полную конфигурацию шрифта.
         *
         * @param id Идентификатор шрифта (должен быть зарегистрирован в resources.json).
         *
         * Контракт:
         *  - ID должен существовать в реестре (проверяется через contains()
         *    или гарантируется инвариантами инициализации/кодогеном).
         *  - Если ID отсутствует → LOG_PANIC.
         *  - Возвращает константную ссылку (read-only).
         *
         * Примечание: на текущем этапе FontConfig содержит только путь.
         */
        [[nodiscard]] static const FontConfig& getFontConfig(ids::FontID id);

        /**
         * @brief Получить полную конфигурацию звукового ресурса (SoundBuffer).
         *
         * @param id Идентификатор звука (должен быть зарегистрирован в resources.json).
         *
         * Контракт:
         *  - ID должен существовать в реестре (проверяется через contains()
         *    или гарантируется инвариантами инициализации/кодогеном).
         *  - Если ID отсутствует → LOG_PANIC.
         *  - Возвращает константную ссылку (read-only).
         *
         * Примечание: на текущем этапе SoundConfig содержит только путь.
         */
        [[nodiscard]] static const SoundConfig& getSoundConfig(ids::SoundID id);

        [[nodiscard]] static bool contains(ids::TextureID id) noexcept;
        [[nodiscard]] static bool contains(ids::FontID id) noexcept;
        [[nodiscard]] static bool contains(ids::SoundID id) noexcept;

        [[nodiscard]] static const std::unordered_map<ids::TextureID, TextureConfig>&
        getAllTextureConfigs() noexcept;

        [[nodiscard]] static const std::unordered_map<ids::FontID, FontConfig>&
        getAllFontConfigs() noexcept;

        [[nodiscard]] static const std::unordered_map<ids::SoundID, SoundConfig>&
        getAllSoundConfigs() noexcept;

      private:
        static std::unordered_map<ids::TextureID, TextureConfig>& getTextureMap();
        static std::unordered_map<ids::FontID, FontConfig>& getFontMap();
        static std::unordered_map<ids::SoundID, SoundConfig>& getSoundMap();
    };

} // namespace core::resources::paths