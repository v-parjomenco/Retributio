// ================================================================================================
// File: core/resources/resource_manager.h
// Purpose: High-level interface for loading and caching engine resources
//          (textures, fonts, sounds) on top of ResourceHolder + ResourcePaths.
// Used by: Game, ECS systems (PlayerInitSystem, DebugOverlaySystem, UI, etc.)
// Related headers: texture_resource.h, font_resource.h, soundbuffer_resource.h, resource_holder.h,
//                  resource_ids.h
// Notes:
//  - Must be called once at boot.
// ================================================================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/resources/holders/resource_holder.h"
#include "core/resources/ids/resource_ids.h"
#include "core/resources/keys/resource_key.h"
#include "core/resources/registry/resource_registry.h"
#include "core/resources/types/font_resource.h"
#include "core/resources/types/soundbuffer_resource.h"
#include "core/resources/types/texture_resource.h"

namespace core::resources {

    /**
     * @brief Высокоуровневый фасад ресурсного слоя.
     *
     * Основная линия развития (Resource Registry v1):
     *  - Key-world API (RuntimeKey32) — целевой путь: O(1) доступ по key.index()
     *    через векторные кэши.
     *
     * Совместимость (LEGACY, будет удалено в PR5):
     *  - Enum-ID API (TextureID / FontID / SoundID) поверх ResourcePaths.
     *
     * Динамические строковые ID/пути:
     *  - Оставлены как гибкий escape hatch (тулы/прототипы/утилиты).
     *  - Для строгих настроек smooth/repeated/mipmap и детерминизма ресурс должен быть в реестре.
     */
    class ResourceManager {
      public:
        ResourceManager() = default;
        ~ResourceManager() = default;

        ResourceManager(const ResourceManager&) = delete;
        ResourceManager& operator=(const ResourceManager&) = delete;
        ResourceManager(ResourceManager&&) = delete;
        ResourceManager& operator=(ResourceManager&&) = delete;

        // ----------------------------------------------------------------------------------------
        // Bootstrapping (реестр ресурсов, LEGACY ResourcePaths)
        // ----------------------------------------------------------------------------------------

        /**
         * @brief Загрузить реестр ресурсов (resources.json) для LEGACY-пути ResourcePaths.
         *
         * Важно:
         *  - операция инициализации; должна вызываться при старте до первого getTexture/getFont/
         *    getSound
         *    по enum-ID;
         *  - ошибки считаются критическими.
         */
        void loadRegistryFromJson(std::string_view filename);

        // ----------------------------------------------------------------------------------------
        // Bootstrapping (key-world, ResourceRegistry v1)
        // ----------------------------------------------------------------------------------------

        /**
         * @brief Инициализация key-world реестра и кэшей.
         *
         * Контракт:
         *  - вызывается ровно один раз на старте (до любых getTexture/getFont/tryGetSound
         *    по ключам);
         *  - повторный вызов = programmer error (LOG_PANIC).
         */
        void initialize(std::span<const ResourceSource> sources);

        // ----------------------------------------------------------------------------------------
        // Key-based API (RuntimeKey32) — целевой путь
        // ----------------------------------------------------------------------------------------

        /**
         * @brief Получить текстуру по RuntimeKey32.
         *
         * Контракт:
         *  - invalid ключ или index вне диапазона => fallback (missingTextureKey());
         *  - любая ошибка загрузки (I/O/декодирование) => LOG_PANIC (Type A).
         *  - загрузка выполняется один раз, далее O(1) через индекс.
         */
        [[nodiscard]] const types::TextureResource& getTexture(TextureKey key);

        /**
         * @brief Получить шрифт по RuntimeKey32.
         *
         * Контракт:
         *  - invalid ключ или index вне диапазона => fallback (missingFontKey());
         *  - любая ошибка загрузки => LOG_PANIC (Type A).
         *  - загрузка выполняется один раз, далее O(1) через индекс.
         */
        [[nodiscard]] const types::FontResource& getFont(FontKey key);

        /**
         * @brief Получить звуковой буфер по RuntimeKey32.
         *
         * Контракт:
         *  - invalid ключ или index вне диапазона => nullptr;
         *  - ошибка загрузки => soft-fail (LOG_WARN один раз + skip); повторно не пробуем.
         *  - загрузка выполняется один раз, далее O(1) через индекс.
         */
        [[nodiscard]] const types::SoundBufferResource* tryGetSound(SoundKey key);

        /// Найти ключ текстуры по каноническому имени (O(log N), для тулов/конфигов).
        [[nodiscard]] TextureKey findTexture(std::string_view canonicalName) const;

        /// Доступ к реестру key-world (для тулов/лоадеров).
        [[nodiscard]] const ResourceRegistry& registry() const noexcept;

        /// Fallback-ключи key-world (валидны после initialize()).
        [[nodiscard]] TextureKey missingTextureKey() const noexcept;
        [[nodiscard]] FontKey missingFontKey() const noexcept;

        // ----------------------------------------------------------------------------------------
        // Текстуры (LEGACY enum-ID)
        // ----------------------------------------------------------------------------------------

        // LEGACY (будет удалено в PR5)
        /// Получить текстуру по static enum-ID.
        const types::TextureResource& getTexture(ids::TextureID id);

        /// Получить текстуру по динамическому строковому идентификатору (временно трактуется
        /// как путь).
        const types::TextureResource& getTexture(const std::string& id);

        /// Получить текстуру по явному пути к файлу (escape hatch).
        const types::TextureResource& getTextureByPath(const std::string& path);

        // ----------------------------------------------------------------------------------------
        // Шрифты (LEGACY enum-ID)
        // ----------------------------------------------------------------------------------------

        // LEGACY (будет удалено в PR5)
        /// Получить шрифт по static enum-ID.
        const types::FontResource& getFont(ids::FontID id);

        /// Получить шрифт по динамическому строковому идентификатору (временно трактуется 
        /// как путь).
        const types::FontResource& getFont(const std::string& id);

        // ----------------------------------------------------------------------------------------
        // Звуки (LEGACY enum-ID)
        // ----------------------------------------------------------------------------------------

        // LEGACY (будет удалено в PR5)
        /// Получить звуковой буфер по static enum-ID (soft-fail, возвращает пустой буфер).
        const types::SoundBufferResource& getSound(ids::SoundID id);

        /// Получить звуковой буфер по динамическому строковому идентификатору 
        /// (soft-fail, пустой буфер).
        const types::SoundBufferResource& getSound(const std::string& id);

        // ----------------------------------------------------------------------------------------
        // Fallback-ресурсы (LEGACY)
        // ----------------------------------------------------------------------------------------

        void setMissingTextureFallback(ids::TextureID id);
        void setMissingFontFallback(ids::FontID id);

        /// Статистика по загруженным ресурсам (LEGACY holders).
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
        // Preload API (LEGACY enum-ID)
        // ----------------------------------------------------------------------------------------

        // LEGACY (будет удалено в PR5)
        void preloadTexture(ids::TextureID id);
        void preloadFont(ids::FontID id);
        void preloadSound(ids::SoundID id);

        void preloadAllTextures();
        void preloadAllFonts();
        void preloadAllSounds();

        // ----------------------------------------------------------------------------------------
        // Preload API (key-world, deterministic by index)
        // ----------------------------------------------------------------------------------------

        void preloadTexture(TextureKey key);
        void preloadFont(FontKey key);
        void preloadSound(SoundKey key);

        void preloadAllTexturesByRegistry();
        void preloadAllFontsByRegistry();
        void preloadAllSoundsByRegistry();

        // ----------------------------------------------------------------------------------------
        // Управление жизненным циклом ресурсов (LEGACY holders + key caches)
        // ----------------------------------------------------------------------------------------

        void unloadTexture(ids::TextureID id) noexcept;
        void unloadTexture(const std::string& id) noexcept;

        void unloadFont(ids::FontID id) noexcept;
        void unloadFont(const std::string& id) noexcept;

        void unloadSound(ids::SoundID id) noexcept;
        void unloadSound(const std::string& id) noexcept;

        void clearAll() noexcept;

        #if defined(SFML1_PROFILE)
        void enableProfileStressTextureDuplication(ids::TextureID sourceId) noexcept;
        void disableProfileStressTextureDuplication() noexcept;
        #endif

      private:
        // ----------------------------------------------------------------------------------------
        // Key-world registry + O(1) vector caches (PR3)
        // ----------------------------------------------------------------------------------------

        ResourceRegistry mRegistry;
        bool mInitialized = false;

        std::vector<std::unique_ptr<types::TextureResource>> mTextureCache;
        std::vector<std::unique_ptr<types::FontResource>> mFontCache;
        std::vector<std::unique_ptr<types::SoundBufferResource>> mSoundCache;

        // Состояния загрузки:
        //  - Texture/Font: 0 = not attempted, 1 = loaded (Type A => ошибка загрузки = PANIC).
        //  - Sound:        0 = not attempted, 1 = loaded, 2 = failed
        //    (soft-fail, больше не пробуем).
        std::vector<std::uint8_t> mTextureState;
        std::vector<std::uint8_t> mFontState;
        std::vector<std::uint8_t> mSoundState;

        TextureKey mMissingTextureKey{};
        FontKey mMissingFontKey{};

        // Реестр ресурсов (resources.json) должен быть загружен ровно один раз на старте (LEGACY).
        bool mRegistryLoaded = false;

        // LEGACY holders (enum-ID).
        holders::ResourceHolder<types::TextureResource, ids::TextureID> mTextures;
        holders::ResourceHolder<types::FontResource, ids::FontID> mFonts;
        holders::ResourceHolder<types::SoundBufferResource, ids::SoundID> mSounds;

        // Dynamic holders (string keys/paths).
        holders::ResourceHolder<types::TextureResource, std::string> mDynamicTextures;
        holders::ResourceHolder<types::FontResource, std::string> mDynamicFonts;
        holders::ResourceHolder<types::SoundBufferResource, std::string> mDynamicSounds;

        // LEGACY fallback IDs.
        bool mHasMissingTextureFallback = false;
        ids::TextureID mMissingTextureID{};

        bool mHasMissingFontFallback = false;
        ids::FontID mMissingFontID{};

        #if defined(SFML1_PROFILE)
            bool mProfileStressTexturesEnabled = false;
            ids::TextureID mProfileStressSourceTextureId{};
        #endif
    };

#if defined(SFML1_TESTS)
    namespace resource_manager::test {
        // Важно: без имён параметров — меньше шанс словить макро/парсинг-коллизии.
        using TextureLoadFn = bool (*)(types::TextureResource&, std::string_view);
        using FontLoadFn = bool (*)(types::FontResource&, std::string_view);
        using SoundLoadFn = bool (*)(types::SoundBufferResource&, std::string_view);

        void setTextureLoadFn(TextureLoadFn fn) noexcept;
        void resetTextureLoadFn() noexcept;
        void setFontLoadFn(FontLoadFn fn) noexcept;
        void resetFontLoadFn() noexcept;
        void setSoundLoadFn(SoundLoadFn fn) noexcept;
        void resetSoundLoadFn() noexcept;
    } // namespace resource_manager::test
#endif // defined(SFML1_TESTS)

} // namespace core::resources