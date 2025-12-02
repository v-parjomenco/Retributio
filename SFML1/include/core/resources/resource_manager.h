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
     *  - Использует ResourcePaths, чтобы сопоставить enum-ID с реальным путём на диске.
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

        /// Каноничный способ получить текстуру: по статическому enum-ID.
        const types::TextureResource& getTexture(ids::TextureID id, bool smooth = true);

        /// Получить текстуру по динамическому строковому ID (сейчас = путь к файлу).
        const types::TextureResource& getTexture(const std::string& id, bool smooth = true);

        /// Получить текстуру по явному пути к файлу (escape hatch, кэшируется как динамическая).
        const types::TextureResource& getTextureByPath(const std::string& path, bool smooth = true);

        // ----------------------------------------------------------------------------------------
        // Шрифты
        // ----------------------------------------------------------------------------------------

        /// Получить шрифт по статическому enum-ID.
        const types::FontResource& getFont(ids::FontID id);

        /// Динамический шрифт по строковому «ID» (сейчас = путь к файлу).
        const types::FontResource& getFont(const std::string& id);

        // ----------------------------------------------------------------------------------------
        // Звуки
        // ----------------------------------------------------------------------------------------

        /// Получить звуковой буфер по статическому enum-ID.
        const types::SoundBufferResource& getSound(ids::SoundID id);

        /// Динамический звук по строковому «ID» (сейчас = путь к файлу).
        const types::SoundBufferResource& getSound(const std::string& id);

        // ----------------------------------------------------------------------------------------
        // Fallback-ресурсы («фиолетовый квадрат» и др.)
        // ----------------------------------------------------------------------------------------

        /**
         * @brief Задать fallback-текстуру, которая будет использоваться, если:
         *        - не найден путь в ResourcePaths;
         *        - текстура не загрузилась с диска;
         *        - или произошла другая ошибка при getTexture(...).
         *
         * Fallback задаётся через TextureID (системная текстура),
         * чтобы оставаться в рамках общей ресурсной системы.
         *
         * При установке fallback-текстура сразу пробуется загрузиться
         * (через getTexture), чтобы поймать ошибки конфигурации на старте.
         */
        void setMissingTextureFallback(ids::TextureID id);

        /// Аналогично для шрифта.
        void setMissingFontFallback(ids::FontID id);

        /// Аналогично для звука.
        void setMissingSoundFallback(ids::SoundID id);

        /// Статистика по загруженным ресурсам.
        /// На этом этапе считаем только количество объектов в кэшах.
        /// В будущем можно расширить данными о памяти, hit/miss и т.п.
        struct ResourceMetrics {
            std::size_t textureCount = 0;
            std::size_t dynamicTextureCount = 0;
            std::size_t fontCount = 0;
            std::size_t dynamicFontCount = 0;
            std::size_t soundCount = 0;
            std::size_t dynamicSoundCount = 0;
        };

        /**
         * @brief Получить простые метрики по ресурсам.
         *
         * На текущем этапе:
         *  - количество статических и динамических текстур;
         *  - количество статических и динамических шрифтов;
         *  - количество статических и динамических звуков.
         *
         * Позже сюда можно добавить:
         *  - суммарное потребление памяти по типам;
         *  - статистику hit/miss по кэшу;
         *  - счётчики загрузок/выгрузок.
         */
        [[nodiscard]] ResourceMetrics getMetrics() const noexcept;

        // ----------------------------------------------------------------------------------------
        // Preload API (для экранов загрузки / подготовки больших сцен)
        // ----------------------------------------------------------------------------------------

        /**
         * @brief Принудительно загрузить текстуру в кэш.
         *
         * Эквивалентно вызову getTexture(id, smooth), но результат явно игнорируется.
         * Удобно использовать при загрузке уровней/сцен.
         */
        void preloadTexture(ids::TextureID id, bool smooth = true) {
            (void) getTexture(id, smooth);
        }

        /**
         * @brief Принудительно загрузить шрифт в кэш.
         */
        void preloadFont(ids::FontID id) {
            (void) getFont(id);
        }

        /**
         * @brief Принудительно загрузить звуковой буфер в кэш.
         */
        void preloadSound(ids::SoundID id) {
            (void) getSound(id);
        }

        /**
         * @brief Предзагрузка всех текстур, описанных в реестре ResourcePaths.
         *
         * Типичный сценарий:
         *  - при показе экрана загрузки уровня;
         *  - при инициализации большого UI.
         *
         * Важно: порядок и набор ID берутся из ResourcePaths::getAllTexturePaths().
         */
        void preloadAllTextures();

        /**
         * @brief Предзагрузка всех шрифтов, описанных в реестре ResourcePaths.
         *
         * Типичный сценарий:
         *  - при показе экрана загрузки уровня;
         *  - при инициализации большого UI.
         *
         * Важно: порядок и набор ID берутся из ResourcePaths::getAllTexturePaths().
         */
        void preloadAllFonts();

        /**
         * @brief Предзагрузка всех звуков, описанных в реестре ResourcePaths.
         *
         * Типичный сценарий:
         *  - при показе экрана загрузки уровня;
         *  - при инициализации большого UI.
         *
         * Важно: порядок и набор ID берутся из ResourcePaths::getAllTexturePaths().
         */
        void preloadAllSounds();

        // ----------------------------------------------------------------------------------------
        // Управление жизненным циклом ресурсов (подготовка к 4X / большим проектам)
        // ----------------------------------------------------------------------------------------

        /// Явно выгрузить ресурсы из кэша (осторожно с висячими ссылками!).
        void unloadTexture(ids::TextureID id) noexcept {
            mTextures.unload(id);
        }
        void unloadTexture(const std::string& id) noexcept {
            mDynamicTextures.unload(id);
        }

        void unloadFont(ids::FontID id) noexcept {
            mFonts.unload(id);
        }
        void unloadFont(const std::string& id) noexcept {
            mDynamicFonts.unload(id);
        }

        void unloadSound(ids::SoundID id) noexcept {
            mSounds.unload(id);
        }
        void unloadSound(const std::string& id) noexcept {
            mDynamicSounds.unload(id);
        }

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
        void clearAll() noexcept {
            mTextures.clear();
            mFonts.clear();
            mSounds.clear();
            mDynamicTextures.clear();
            mDynamicFonts.clear();
            mDynamicSounds.clear();
        }

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

        // ------------------------------------------------------------------------------------
        // TODO (будущие расширения ResourceManager для 4X / больших проектов):
        //  - Потокобезопасность и асинхронная загрузка (background streaming).
        //  - LRU-кэш и бюджеты по памяти для текстур/звукa.
        //  - Поддержка модов/DLC и override-путей поверх базового реестра.
        //  - Более богатые метрики (память, hit/miss, профилирование).
        // ------------------------------------------------------------------------------------
    };

} // namespace core::resources
