// ================================================================================================
// File: core/resources/resource_manager.h
// Purpose: High-level interface for loading and caching engine resources
//          (textures, fonts, sounds) on top of ResourceHolder + ResourceRegistry.
// Used by: Game, ECS systems (PlayerInitSystem, DebugOverlaySystem, UI, etc.)
// Related headers: texture_resource.h, font_resource.h, soundbuffer_resource.h, resource_holder.h
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
#include "core/resources/keys/resource_key.h"
#include "core/resources/registry/resource_registry.h"
#include "core/resources/types/font_resource.h"
#include "core/resources/types/soundbuffer_resource.h"
#include "core/resources/types/texture_resource.h"

namespace core::resources {

    /**
     * @brief Высокоуровневый фасад ресурсного слоя.
     * 
     *  - Key-world API (RuntimeKey32) — целевой путь: O(1) доступ по key.index()
     *    через векторные кэши.
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

        /// Найти ключ шрифта по каноническому имени (O(log N), для тулов/конфигов).
        [[nodiscard]] FontKey findFont(std::string_view canonicalName) const;

        /// Доступ к реестру key-world (для тулов/лоадеров).
        [[nodiscard]] const ResourceRegistry& registry() const noexcept;

        /// Fallback-ключи key-world (валидны после initialize()).
        [[nodiscard]] TextureKey missingTextureKey() const noexcept;
        [[nodiscard]] FontKey missingFontKey() const noexcept;

        // ----------------------------------------------------------------------------------------
        // Динамические пути (escape hatch)
        // ----------------------------------------------------------------------------------------

        /// Получить текстуру по динамическому строковому идентификатору.
        const types::TextureResource& getTexture(const std::string& id);

        /// Получить текстуру по явному пути к файлу (escape hatch).
        const types::TextureResource& getTextureByPath(const std::string& path);

        /// Получить шрифт по динамическому строковому идентификатору.
        const types::FontResource& getFont(const std::string& id);

        /// Получить звуковой буфер по динамическому строковому идентификатору.
        const types::SoundBufferResource& getSound(const std::string& id);

        // ----------------------------------------------------------------------------------------
        // Метрики
        // ----------------------------------------------------------------------------------------

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
        // Preload API (key-world, deterministic by index)
        // ----------------------------------------------------------------------------------------

        void preloadTexture(TextureKey key);
        void preloadFont(FontKey key);
        void preloadSound(SoundKey key);

        void preloadAllTexturesByRegistry();
        void preloadAllFontsByRegistry();
        void preloadAllSoundsByRegistry();

        // ----------------------------------------------------------------------------------------
        // Управление жизненным циклом ресурсов (key caches + dynamic holders)
        // ----------------------------------------------------------------------------------------

        void unloadTexture(const std::string& id) noexcept;
        void unloadFont(const std::string& id) noexcept;
        void unloadSound(const std::string& id) noexcept;
        void clearAll() noexcept;

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

        // Dynamic holders (string keys/paths).
        holders::ResourceHolder<types::TextureResource, std::string> mDynamicTextures;
        holders::ResourceHolder<types::FontResource, std::string> mDynamicFonts;
        holders::ResourceHolder<types::SoundBufferResource, std::string> mDynamicSounds;

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