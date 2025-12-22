// ================================================================================================
// File: core/resources/resource_manager.h
// Purpose: High-level interface for loading and caching engine resources
//          (textures, fonts, sounds) on top of ResourceHolder + ResourcePaths.
// Used by: Game, ECS systems (PlayerInitSystem, DebugOverlaySystem, UI, etc.)
// Related headers: texture_resource.h, font_resource.h, soundbuffer_resource.h, resource_holder.h,
//                  resource_ids.h, resource_paths.h
// ================================================================================================
#pragma once

#include <string>

#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>

#include "core/resources/holders/resource_holder.h"
#include "core/resources/ids/resource_ids.h"
#include "core/resources/paths/resource_paths.h"
#include "core/resources/types/font_resource.h"
#include "core/resources/types/soundbuffer_resource.h"
#include "core/resources/types/texture_resource.h"

namespace core::resources {

    /**
     * @brief Высокоуровневый фасад над ResourceHolder + ResourcePaths.
     *
     * Обязанности:
     *  - Предоставляет простой API для запроса ресурсов по enum-ID
     *    (TextureID / FontID / SoundID).
     *  - Использует ResourcePaths, чтобы сопоставить enum-ID с реальным путём на диске
     *    и конфигурацией (smooth/repeated/mipmap).
     *  - Кэширует загруженные ресурсы и гарантирует, что каждый файл грузится с диска
     *    не более одного раза.
     *
     * Поддерживаются три режима доступа:
     *  1) Статические enum-ID       (TextureID / FontID / SoundID) — каноничный путь для движка.
     *  2) Динамические строковые ID (std::string)                  — для тулов, модов, прототипов.
     *  3) Явные пути к файлам       (getTextureByPath)             — низкоуровневый escape hatch.
     *
     * Ядро движка (геймплей, ECS-системы и т.п.) должно по максимуму использовать
     * вариант №1 — enum-ID + ResourcePaths. Остальные методы — вспомогательные.
     *
     * Для динамических строковых текстур используется единая политика отображения
     * (например, smooth=true по умолчанию). Если нужен полный контроль над smooth/repeated/mipmap,
     * ресурс должен быть описан в resources.json и запрашиваться по Enum-ID.
     * 
     * На уровне 4X/больших стратегий поверх этого слоя могут добавляться:
     *  - стриминговые менеджеры (подкачка чанков);
     *  - LRU-кэши, разгружающие редко используемые ресурсы.
     * Интерфейс ResourceManager оставлен максимально простым и стабильным.
     */
    class ResourceManager {
      public:
        ResourceManager() = default;
        ~ResourceManager() = default;

        // ----------------------------------------------------------------------------------------
        // Текстуры
        // ----------------------------------------------------------------------------------------

        /**
         * @brief Каноничный способ получить текстуру: по статическому enum-ID.
         *
         * Все настройки (smooth/repeated/mipmap) берутся из конфигурации
         * ResourcePaths::getTextureConfig(TextureID) и применяются один раз при первой загрузке.
         */
        const types::TextureResource& getTexture(ids::TextureID id);

        /**
         * @brief Получить текстуру по динамическому строковому идентификатору.
         *
         * Семантика:
         *  - ключом выступает именно строковый ID (логическое имя ресурса);
         *  - метод предназначен для тулов, моддинга, прототипов и других "гибких" потребителей.
         *  - флаги smooth/repeated/mipmap не задаются извне — при необходимостиих нужно
         *    зафиксировать через Enum+resources.json
         *
         * Текущая реализация:
         *  - сейчас строковый ID совпадает с путём к файлу и напрямую передаётся в загрузчик;
         *  - это считается внутренней деталью и может быть изменено в будущем
         *    (например, появится отдельный реестр string-ID → path).
         *
         * В отличие от getTextureByPath(), этот метод говорит: "у меня есть динамический
         * идентификатор", а не "вот конкретный путь на файловой системе".
         */
        const types::TextureResource& getTexture(const std::string& id);

        /**
         * @brief Получить текстуру по явному пути к файлу.
         *
         * Семантика:
         *  - вызывающий сознательно работает с физическим путём на диске;
         *  - ключом в кэше служит сам путь (std::string path).
         *  - флаги smooth/repeated/mipmap не задаются извне — при необходимости их нужно
         *    зафиксировать через Enum+resources.json
         *
         * Назначение:
         *  - низкоуровневый escape hatch для дебага, прототипов, утилит;
         *  - обход любых схем ID/реестров (ResourcePaths, string-ID и т.п.).
         *
         * В отличие от getTexture(const std::string& id), этот метод жёстко привязан к пути и
         * не предполагает переезда на "логические" string-ID.
         */
        const types::TextureResource& getTextureByPath(const std::string& path);

        // ----------------------------------------------------------------------------------------
        // Шрифты
        // ----------------------------------------------------------------------------------------

        /// Получить шрифт по статическому enum-ID.
        const types::FontResource& getFont(ids::FontID id);

        /**
         * @brief Получить шрифт по динамическому строковому идентификатору.
         *
         * Семантика:
         *  - ключом выступает строковый ID (логическое имя шрифта);
         *  - метод предназначен главным образом для тулов, моддинга,
         *    прототипов и тестов.
         *
         * Текущая реализация:
         *  - строковый ID напрямую трактуется как путь к файлу;
         *  - это деталь реализации и может быть изменено в будущем
         *    (например, появится отдельный реестр string-ID → path).
         */
        const types::FontResource& getFont(const std::string& id);

        // ----------------------------------------------------------------------------------------
        // Звуки
        // ----------------------------------------------------------------------------------------

        /// Получить звуковой буфер по статическому enum-ID.
        const types::SoundBufferResource& getSound(ids::SoundID id);

        /**
         * @brief Получить звуковой буфер по динамическому строковому идентификатору.
         *
         * Семантика:
         *  - ключом в кэше является строковый ID (логическое имя звука);
         *  - ориентировано на тулы, моддинг и гибкие сценарии.
         *
         * Текущая реализация:
         *  - строковый ID сейчас совпадает с путём к файлу и передаётся
         *    напрямую в загрузчик;
         *  - это считается внутренней деталью и в будущем может измениться
         *    (например, будет карта string-ID → path).
         */
        const types::SoundBufferResource& getSound(const std::string& id);

        // ----------------------------------------------------------------------------------------
        // Fallback-ресурсы («фиолетовый квадрат» и др.)
        // ----------------------------------------------------------------------------------------

        void setMissingTextureFallback(ids::TextureID id);
        void setMissingFontFallback(ids::FontID id);
        void setMissingSoundFallback(ids::SoundID id);

        /// Статистика по загруженным ресурсам.
        struct ResourceMetrics {
            std::size_t textureCount = 0;
            std::size_t dynamicTextureCount = 0;
            std::size_t fontCount = 0;
            std::size_t dynamicFontCount = 0;
            std::size_t soundCount = 0;
            std::size_t dynamicSoundCount = 0;
        };

        [[nodiscard]] ResourceMetrics getMetrics() const noexcept;

        // ----------------------------------------------------------------------------------------
        // Preload API (для экранов загрузки / подготовки больших сцен)
        // ----------------------------------------------------------------------------------------

        /// Принудительно загрузить текстуру в кэш (по EnumID).
        void preloadTexture(ids::TextureID id);

        /// Принудительно загрузить шрифт в кэш (по EnumID).
        void preloadFont(ids::FontID id);

        /// Принудительно загрузить звуковой буфер в кэш (по EnumID).
        void preloadSound(ids::SoundID id);

        /// Предзагрузка всех текстур, описанных в реестре ResourcePaths.
        void preloadAllTextures();

        /// Предзагрузка всех шрифтов, описанных в реестре ResourcePaths.
        void preloadAllFonts();

        /// Предзагрузка всех звуков, описанных в реестре ResourcePaths.
        void preloadAllSounds();

        // ----------------------------------------------------------------------------------------
        // Управление жизненным циклом ресурсов (подготовка к 4X / большим проектам)
        // ----------------------------------------------------------------------------------------

        /// Явно выгрузить ресурсы из кэша (осторожно с висячими ссылками!).
        void unloadTexture(ids::TextureID id) noexcept;
        void unloadTexture(const std::string& id) noexcept;

        void unloadFont(ids::FontID id) noexcept;
        void unloadFont(const std::string& id) noexcept;

        void unloadSound(ids::SoundID id) noexcept;
        void unloadSound(const std::string& id) noexcept;

        /**
         * @brief Полностью очистить все кэши ресурсов данного менеджера.
         *
         * Типичный сценарий:
         *  - полная перезагрузка игры;
         *  - переключение на другой крупный сценарий/кампанию;
         *  - unit-тесты, где нужно гарантированно чистое состояние.
         *
         * Важно: fallback-ID остаются, но их ресурсы будут при необходимости
         * пере-загружены при следующем обращении к getTexture/getFont/getSound.
         */
        void clearAll() noexcept;

        #if defined(SFML1_PROFILE)
        // ----------------------------------------------------------------------------------------
        // Диагностика только для PROFILE-сборки:
        //  - включает "виртуальные" текстуры для стресс-тестов, даже если resources.json содержит
        //    только 1 текстуру;
        //  - при включении неизвестные значения TextureID преобразуются в копии текстуры sourceId,
        //    загруженные в mDynamicTextures под уникальными ключами.
        //  - не компилируется в Debug/Release.
        // ----------------------------------------------------------------------------------------
        void enableProfileStressTextureDuplication(ids::TextureID sourceId) noexcept;
        void disableProfileStressTextureDuplication() noexcept;
        #endif

      private:
        // Хранилища для статических enum-ID (основной путь для движка).
        holders::ResourceHolder<types::TextureResource, ids::TextureID> mTextures;
        holders::ResourceHolder<types::FontResource, ids::FontID> mFonts;
        holders::ResourceHolder<types::SoundBufferResource, ids::SoundID> mSounds;

        // Хранилища для динамических строковых ключей (тулы / моддинг / явные пути).
        holders::ResourceHolder<types::TextureResource, std::string> mDynamicTextures;
        holders::ResourceHolder<types::FontResource, std::string> mDynamicFonts;
        holders::ResourceHolder<types::SoundBufferResource, std::string> mDynamicSounds;

        // Fallback-ресурсы (храним ID, а не указатели, чтобы не ловить висячие ссылки).
        bool mHasMissingTextureFallback = false;
        ids::TextureID mMissingTextureID{};

        bool mHasMissingFontFallback = false;
        ids::FontID mMissingFontID{};

        bool mHasMissingSoundFallback = false;
        ids::SoundID mMissingSoundID{};

        #if defined(SFML1_PROFILE)
            bool mProfileStressTexturesEnabled = false;
            ids::TextureID mProfileStressSourceTextureId{};
        #endif

        // ----------------------------------------------------------------------------------------
        // TODO (будущие расширения ResourceManager для 4X / больших проектов):
        //  - Потокобезопасность и асинхронная загрузка (background streaming).
        //  - LRU-кэши и бюджеты по памяти для текстур/звукa.
        //  - Поддержка модов/DLC и override-путей поверх базового реестра.
        //  - Более богатые метрики (память, hit/miss, профилирование).
        //
        //  - Hook для hot-reload/streaming: логика переинициализации ресурсов по сигналу редактора
        //    или файлового watcher'а должна жить на уровне ResourceManager, а не в низкоуровневых
        //    holders.
        // ----------------------------------------------------------------------------------------
    };

} // namespace core::resources
