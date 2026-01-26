// ================================================================================================
// File: core/resources/resource_manager.h
// Purpose: High-level interface for loading and caching engine resources
//          (textures, fonts, sounds) on top of ResourceRegistry.
// Used by: Game, ECS systems (PlayerInitSystem, DebugOverlaySystem, UI, etc.)
// Related headers: texture_resource.h, font_resource.h, soundbuffer_resource.h, resource_registry.h
// Notes:
//  - Must be called once at boot.
// ================================================================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#if defined(SFML1_TESTS)
    #include <filesystem>
#endif

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

        /**
         * @brief Запретить lazy-load после фазы инициализации/прелоада.
         *
         * Контракт:
         *  - Должно быть включено после того, как сцена/ресурсный набор подготовлены.
         *  - Любая попытка загрузить ресурс через loading API после этого — programmer error.
         */
        void setIoForbidden(bool enabled) noexcept;

        /**
         * @brief Поколение кэша ресурсов (для инвалидирования внешних pointer-кэшей).
         *
         * Инкрементируется при initialize()/clearAll() и будущих reload/eviction действиях.
         */
        [[nodiscard]] std::uint32_t cacheGeneration() const noexcept;

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

        // ----------------------------------------------------------------------------------------
        // Resident-only API (NO I/O, NO decoding, NO allocations)
        // ----------------------------------------------------------------------------------------

        /**
         * @brief Получить resident текстуру. НИКОГДА не делает I/O.
         *
         * Контракт:
         *  - invalid/out-of-range => missingTextureKey (missing гарантированно resident).
         *  - если ресурс не resident => LOG_PANIC (programmer error: забыли preload).
         */
        [[nodiscard]] const types::TextureResource& expectTextureResident(TextureKey key) const;

        /**
         * @brief Получить resident шрифт. НИКОГДА не делает I/O.
         *
         * Контракт:
         *  - invalid/out-of-range => missingFontKey (missing гарантированно resident).
         *  - если ресурс не resident => LOG_PANIC (programmer error: забыли preload).
         */
        [[nodiscard]] const types::FontResource& expectFontResident(FontKey key) const;

        /**
         * @brief Получить resident звук, если он есть. НИКОГДА не делает I/O.
         *
         * Политика: sounds soft-fail (nullptr означает "нет звука/не загружен/сломался").
         */
        [[nodiscard]] const types::SoundBufferResource* 
            tryGetSoundResident(SoundKey key) const noexcept;

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
        // Метрики
        // ----------------------------------------------------------------------------------------

        /// Статистика по загруженным ресурсам key-world.
        struct ResourceMetrics {
            std::size_t textureCount = 0;
            std::size_t fontCount = 0;
            std::size_t soundCount = 0;
        };

        [[nodiscard]] ResourceMetrics getMetrics() const noexcept;

        // Batch preload (scene-level)
        void preloadTextures(std::span<const TextureKey> keys);
        void preloadFonts(std::span<const FontKey> keys);
        void preloadSounds(std::span<const SoundKey> keys);

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
        // Key-world cache control
        // ----------------------------------------------------------------------------------------

        void clearAll() noexcept;

      private:

        enum class ResourceState : std::uint8_t {
            NotLoaded = 0,
            Resident  = 1,
            Failed    = 2
            // NOTE: extend with Loading/Unloaded when async loading / eviction lands.
        };

        void bumpCacheGeneration() noexcept;

        // ----------------------------------------------------------------------------------------
        // Key-world registry + O(1) vector caches (PR3)
        // ----------------------------------------------------------------------------------------

        ResourceRegistry mRegistry;
        bool mInitialized = false;
        bool mIoForbidden = false;
        std::uint32_t mCacheGeneration = 0;

        // Примечание:
        //  - Кэши хранят ptr для ленивой загрузки и будущего streaming/eviction.
        //  - Если позже появится churn/фрагментация — переоценить layout (slot/pool/in-place).
        std::vector<std::unique_ptr<types::TextureResource>> mTextureCache;
        std::vector<std::unique_ptr<types::FontResource>> mFontCache;
        std::vector<std::unique_ptr<types::SoundBufferResource>> mSoundCache;

        // Состояния загрузки (single-threaded for now):
        //  - Texture/Font: NotLoaded -> Resident (Type A => ошибка загрузки = PANIC).
        //  - Sound:        NotLoaded -> Resident | Failed (soft-fail, stop retries).
        std::vector<ResourceState> mTextureState;
        std::vector<ResourceState> mFontState;
        std::vector<ResourceState> mSoundState;

        TextureKey mMissingTextureKey{};
        FontKey mMissingFontKey{};
    };

#if defined(SFML1_TESTS)
    namespace resource_manager::test {
        // Важно: без имён параметров — меньше шанс словить макро/парсинг-коллизии.
        using TextureLoadFn = bool (*)(types::TextureResource&, const std::filesystem::path&,
                                       bool sRgb);
        using FontLoadFn = bool (*)(types::FontResource&, const std::filesystem::path&);
        using SoundLoadFn = bool (*)(types::SoundBufferResource&, const std::filesystem::path&);

        void setTextureLoadFn(TextureLoadFn fn) noexcept;
        void resetTextureLoadFn() noexcept;
        void setFontLoadFn(FontLoadFn fn) noexcept;
        void resetFontLoadFn() noexcept;
        void setSoundLoadFn(SoundLoadFn fn) noexcept;
        void resetSoundLoadFn() noexcept;
    } // namespace resource_manager::test
#endif // defined(SFML1_TESTS)

} // namespace core::resources
