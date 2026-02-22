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
#include <functional>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

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
        // ----------------------------------------------------------------------------------------
        // I/O seam — инъекция загрузчиков для initialize().
        //
        // Контракт:
        //  - Поля по умолчанию пусты ({}), тогда используется продакшн-загрузка с диска.
        //  - Продакшн callsite ничего не передаёт (Loaders{} по умолчанию).
        //  - Тесты передают стабы напрямую; глобального mutable state нет; SFML1_TESTS нет.
        //
        // Важно:
        //  - В публичном заголовке НЕ используем std::filesystem::path, чтобы не тащить
        //    <filesystem> как обязательную зависимость продукта (compile-time/границы).
        //  - Путь передаётся как std::string_view; .cpp конвертирует в filesystem::path
        //    только в продакшн-default пути.
        //
        // Loaders хранится в members, потому что lazy-load возможен после initialize().
        // ----------------------------------------------------------------------------------------
        struct Loaders {
            using TextureFn = std::function<bool(types::TextureResource&,
                                                 std::string_view path,
                                                 bool sRgb)>;
            using FontFn    = std::function<bool(types::FontResource&,
                                                 std::string_view path)>;
            using SoundFn   = std::function<bool(types::SoundBufferResource&,
                                                 std::string_view path)>;

            TextureFn texture{}; // empty → default file-based loader
            FontFn    font{};    // empty → default file-based loader
            SoundFn   sound{};   // empty → default file-based loader
        };

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
         * @param sources  Список JSON-файлов реестра (см. ResourceRegistry::loadFromSources).
         * @param loaders  Опциональные перегрузки загрузчиков. Продакшн-код не передаёт их
         *                 (умолчание Loaders{} → файловые загрузчики).
         *
         * Контракт:
         *  - вызывается ровно один раз на старте (до любых getTexture/getFont/tryGetSound);
         *  - повторный вызов = programmer error (LOG_PANIC).
         */
        void initialize(std::span<const ResourceSource> sources, Loaders loaders = {});

        /**
         * @brief Запретить lazy-load после фазы инициализации/прелоада.
         */
        void setIoForbidden(bool enabled) noexcept;

        /**
         * @brief Поколение кэша ресурсов (для инвалидирования внешних pointer-кэшей).
         */
        [[nodiscard]] std::uint32_t cacheGeneration() const noexcept;

        // ----------------------------------------------------------------------------------------
        // Key-based API (RuntimeKey32) — целевой путь
        // ----------------------------------------------------------------------------------------

        [[nodiscard]] const types::TextureResource& getTexture(TextureKey key);
        [[nodiscard]] const types::FontResource& getFont(FontKey key);
        [[nodiscard]] const types::SoundBufferResource* tryGetSound(SoundKey key);

        // ----------------------------------------------------------------------------------------
        // Resident-only API (NO I/O, NO decoding, NO allocations)
        // ----------------------------------------------------------------------------------------

        [[nodiscard]] const types::TextureResource& expectTextureResident(TextureKey key) const;
        [[nodiscard]] const types::FontResource& expectFontResident(FontKey key) const;
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

        struct ResourceMetrics {
            std::size_t textureCount = 0;
            std::size_t fontCount = 0;
            std::size_t soundCount = 0;
        };

        [[nodiscard]] ResourceMetrics getMetrics() const noexcept;

        // ----------------------------------------------------------------------------------------
        // Batch preload (scene-level)
        // ----------------------------------------------------------------------------------------

        void preloadTextures(std::span<const TextureKey> keys);
        void preloadFonts(std::span<const FontKey> keys);
        void preloadSounds(std::span<const SoundKey> keys);

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
        enum class ResourceState : std::uint8_t { NotLoaded = 0, Resident = 1, Failed = 2 };

        void bumpCacheGeneration() noexcept;

        // Key-world registry + O(1) vector caches
        ResourceRegistry mRegistry;
        Loaders mLoaders; // инъекция initialize(), используется в lazy-load
        bool mInitialized = false;
        bool mIoForbidden = false;
        std::uint32_t mCacheGeneration = 0;

        std::vector<std::unique_ptr<types::TextureResource>> mTextureCache;
        std::vector<std::unique_ptr<types::FontResource>> mFontCache;
        std::vector<std::unique_ptr<types::SoundBufferResource>> mSoundCache;

        std::vector<ResourceState> mTextureState;
        std::vector<ResourceState> mFontState;
        std::vector<ResourceState> mSoundState;

        TextureKey mMissingTextureKey{};
        FontKey mMissingFontKey{};
    };

} // namespace core::resources