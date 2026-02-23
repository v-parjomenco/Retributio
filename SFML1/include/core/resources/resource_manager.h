// ================================================================================================
// File: core/resources/resource_manager.h
// Purpose: High-level interface for loading and caching engine resources
//          (textures, fonts, sounds) on top of ResourceRegistry.
// Used by: Game, ECS systems (PlayerInitSystem, DebugOverlaySystem, UI, etc.)
// Related headers: texture_resource.h, font_resource.h, resource_registry.h
// Notes:
//  - Must be called once at boot.
//  - NOT thread-safe by design. ResourceManager follows a strict two-phase model:
//      Phase 1 (load): initialize(), preload*(), lazy getTexture/getFont/tryGetSound.
//      Phase 2 (frozen): setIoForbidden(true) → all caches are read-only.
//    After setIoForbidden(true), concurrent reads of resident resources are safe
//    as long as no mutation methods are called. Full MT job-system integration is
//    a future concern; single-threaded load is the current contract.
// ================================================================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "core/resources/handles/sound_handle.h"
#include "core/resources/keys/resource_key.h"
#include "core/resources/registry/resource_registry.h"
#include "core/resources/types/font_resource.h"
#include "core/resources/types/i_sound_resource.h"
#include "core/resources/types/texture_resource.h"

namespace core::resources {

    /**
     * @brief Высокоуровневый фасад ресурсного слоя.
     *
     *  - Key-world API (RuntimeKey32) — целевой путь: O(1) доступ по key.index()
     *    через векторные кэши.
     *
     *  - Playback: ResourceManager отвечает за загрузку и кэширование звуковых ресурсов,
     *    но НЕ за их воспроизведение. Playback — ответственность AudioPlaybackSystem /
     *    AudioService, реализуемого в backend-модуле (sfml1_sfml_audio). Game-код
     *    получает SoundHandle и передаёт его в playback-систему; downcast
     *    ISoundResource → SfmlSoundResource происходит внутри backend, не в core и не
     *    в game. Текущий контракт: tryGetSoundResource(SoundHandle) → const ISoundResource*
     *    для чтения backend-модулем.
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
            // ------------------------------------------------------------------------------------
            // Delegate: атомарная пара (void* user, FnPtr method).
            //
            // Инвариант enforced типом:
            //  - user, method, assign() — private.
            //  - Единственный мутатор — assign(), доступный только Loaders через friend.
            //  - Внешний код не может обновить одно поле без другого — даже случайно.
            //  - assign(u, nullptr) → user тоже обнуляется: единственное "пустое" состояние
            //    это operator bool() == false с обоими полями == nullptr.
            //
            // Размер на x64: ровно 16 байт (два указателя), нет heap-аллокаций.
            // Call-overhead: один indirect call через function pointer (тонет в I/O).
            // ------------------------------------------------------------------------------------
            template<typename Ret, typename... Args>
            struct Delegate {
                using FnPtr = Ret (*)(void*, Args...);

                // Precondition: method != nullptr.
                // Always guard with `if (delegate)` before calling.
                Ret operator()(Args... args) const {
                    return method(user, args...);
                }

                explicit operator bool() const noexcept { return method != nullptr; }

              private:
                // Only Loaders may mutate a Delegate: half-initialised state
                // (method set without user or vice versa) is structurally impossible
                // outside this class pair.
                friend struct Loaders;

                // assign() enforces the invariant: if m is null, user is also zeroed,
                // so operator bool() == false is the sole observable "empty" state.
                void assign(void* u, FnPtr m) noexcept {
                    user   = (m != nullptr) ? u : nullptr;
                    method = m;
                }

                void* user   = nullptr;
                FnPtr method = nullptr;
            };

            // ------------------------------------------------------------------------------------
            // Zero-allocation callbacks (no <functional> in public headers).
            //
            // Threading:
            //  - ResourceManager is intentionally NOT thread-safe. Load phase is single-threaded.
            //
            // Lifetime:
            //  - User pointers must remain valid for the entire lifetime of ResourceManager.
            //    Dangling user pointers are a contract violation and lead to UB.
            //
            // Semantics:
            //  - texture    (empty)  -> default file-based texture loader.
            //  - font       (empty)  -> default file-based font loader.
            //  - createSound (empty) -> sound loading disabled (soft-fail with WARN once).
            //
            // Type-safety usage:
            //  - Prefer bind* helpers below; do not scatter casts at callsites.
            // ------------------------------------------------------------------------------------
            Delegate<bool, types::TextureResource&, std::string_view, bool> texture;
            Delegate<bool, types::FontResource&, std::string_view>          font;
            Delegate<std::unique_ptr<ISoundResource>>                       createSound;

            // ------------------------------------------------------------------------------------
            // Type-safe binders (no user-side casts).
            //
            // Fn can be either:
            //  - member function pointer:
            //      bool (T::*)(TextureResource&, string_view, bool)
            //      bool (T::*)(FontResource&, string_view)
            //      std::unique_ptr<ISoundResource> (T::*)()
            //  - free/static function:
            //      bool (*)(T&, TextureResource&, string_view, bool)
            //      bool (*)(T&, FontResource&, string_view)
            //      std::unique_ptr<ISoundResource> (*)(T&)
            //
            // IMPORTANT:
            //  - A runtime binder that stores function pointers as values is intentionally NOT
            //    provided: making that both portable and type-safe without std::function would
            //    require UB tricks.
            // ------------------------------------------------------------------------------------
            template<typename T, auto Fn>
            void bindTexture(T& obj) noexcept {
                texture.assign(&obj, &textureThunk<T, Fn>);
            }

            template<typename T, auto Fn>
            void bindFont(T& obj) noexcept {
                font.assign(&obj, &fontThunk<T, Fn>);
            }

            template<typename T, auto Fn>
            void bindSoundFactory(T& obj) noexcept {
                createSound.assign(&obj, &soundThunk<T, Fn>);
            }

            template<auto Fn>
            void bindSoundFactory() noexcept {
                createSound.assign(nullptr, &soundThunkStateless<Fn>);
            }

          private:
            template<typename T, auto Fn>
            static bool textureThunk(void* user,
                                     types::TextureResource& out,
                                     std::string_view path,
                                     bool sRgb) {
                auto* obj = static_cast<T*>(user);

                if constexpr (requires { (obj->*Fn)(out, path, sRgb); }) {
                    return (obj->*Fn)(out, path, sRgb);
                } else {
                    static_assert(
                        requires(T& o,
                                 types::TextureResource& r,
                                 std::string_view p) { Fn(o, r, p, true); },
                        "bindTexture requires member Fn or free Fn(T&, TextureResource&, "
                        "string_view, bool).");
                    return Fn(*obj, out, path, sRgb);
                }
            }

            template<typename T, auto Fn>
            static bool fontThunk(void* user,
                                  types::FontResource& out,
                                  std::string_view path) {
                auto* obj = static_cast<T*>(user);

                if constexpr (requires { (obj->*Fn)(out, path); }) {
                    return (obj->*Fn)(out, path);
                } else {
                    static_assert(
                        requires(T& o,
                                 types::FontResource& r,
                                 std::string_view p) { Fn(o, r, p); },
                        "bindFont requires member Fn or free Fn(T&, FontResource&, string_view).");
                    return Fn(*obj, out, path);
                }
            }

            template<typename T, auto Fn>
            static std::unique_ptr<ISoundResource> soundThunk(void* user) {
                auto* obj = static_cast<T*>(user);

                if constexpr (requires { (obj->*Fn)(); }) {
                    return (obj->*Fn)();
                } else {
                    static_assert(requires(T& o) { Fn(o); },
                                  "bindSoundFactory(T&) requires member Fn() or free Fn(T&).");
                    return Fn(*obj);
                }
            }

            template<auto Fn>
            static std::unique_ptr<ISoundResource> soundThunkStateless(void* /* user */) {
                static_assert(requires { Fn(); },
                              "bindSoundFactory() requires free Fn() returning "
                              "std::unique_ptr<ISoundResource>.");
                return Fn();
            }
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
         *                 (умолчание Loaders{} → файловые загрузчики). Аудио-бекенд передаёт
         *                 результат core::resources::backends::sfml_audio::makeSfmlAudioLoader().
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
        [[nodiscard]] SoundHandle tryGetSound(SoundKey key);

        // ----------------------------------------------------------------------------------------
        // Resident-only API (NO I/O, NO decoding, NO allocations)
        // ----------------------------------------------------------------------------------------

        [[nodiscard]] const types::TextureResource& expectTextureResident(TextureKey key) const;
        [[nodiscard]] const types::FontResource& expectFontResident(FontKey key) const;
        [[nodiscard]] SoundHandle tryGetSoundResident(SoundKey key) const noexcept;

        /**
         * @brief Получить указатель на загруженный ISoundResource по хэндлу.
         *
         * Возвращает nullptr если хэндл невалиден или ресурс не в состоянии Resident.
         *
         * Lifetime: указатель валиден до ближайшего вызова clearAll() или смены
         * cacheGeneration(). Не кэшируй указатель дольше одного кадра без проверки
         * cacheGeneration(). Используй SoundHandle как долгоживущий идентификатор.
         */
        [[nodiscard]] const ISoundResource* tryGetSoundResource(
            SoundHandle handle) const noexcept;

        /// Найти ключ текстуры по каноническому имени (O(log N), для тулов/конфигов).
        [[nodiscard]] TextureKey findTexture(std::string_view canonicalName) const;
        /// Найти ключ шрифта по каноническому имени (O(log N), для тулов/конфигов).
        [[nodiscard]] FontKey findFont(std::string_view canonicalName) const;
        /// Найти ключ звука по каноническому имени (O(log N), для тулов/конфигов).
        [[nodiscard]] SoundKey findSound(std::string_view canonicalName) const;

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
        void mergeLoaders(const Loaders& loaders);

        // Key-world registry + O(1) vector caches
        ResourceRegistry mRegistry;
        Loaders mLoaders; // инъекция initialize(), используется в lazy-load
        bool mInitialized = false;
        bool mIoForbidden = false;
        // Один WARN при первом обращении к незарегистрированному sound loader.
        // Не atomic: ResourceManager намеренно single-threaded (см. заголовок файла).
        bool mMissingSoundLoaderWarned = false;
        std::uint32_t mCacheGeneration = 0;

        // Инкрементальные счётчики resident-ресурсов — validate-on-write.
        // Обновляются только в точках перехода состояния (NotLoaded→Resident, clearAll).
        // getMetrics() — O(1), не O(N).
        std::uint32_t mTextureResidentCount = 0;
        std::uint32_t mFontResidentCount    = 0;
        std::uint32_t mSoundResidentCount   = 0;

        std::vector<std::unique_ptr<types::TextureResource>> mTextureCache;
        std::vector<std::unique_ptr<types::FontResource>> mFontCache;
        std::vector<std::unique_ptr<ISoundResource>> mSoundCache;

        std::vector<ResourceState> mTextureState;
        std::vector<ResourceState> mFontState;
        std::vector<ResourceState> mSoundState;

        TextureKey mMissingTextureKey{};
        FontKey mMissingFontKey{};
    };

} // namespace core::resources