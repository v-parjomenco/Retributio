#include "pch.h"

#include "core/resources/resource_manager.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <vector>

#include "core/log/log_macros.h"
#include "core/resources/paths/resource_paths.h"

namespace core::resources {

    namespace {

        static constexpr std::uint8_t kStateNotAttempted = static_cast<std::uint8_t>(0);
        static constexpr std::uint8_t kStateLoaded = static_cast<std::uint8_t>(1);
        static constexpr std::uint8_t kStateFailed = static_cast<std::uint8_t>(2);

#if defined(SFML1_TESTS)
        using TextureLoadFn = resource_manager::test::TextureLoadFn;
        using FontLoadFn = resource_manager::test::FontLoadFn;
        using SoundLoadFn = resource_manager::test::SoundLoadFn;
#endif

        [[nodiscard]] bool loadTextureFromFile(types::TextureResource& out, std::string_view path) {
            return out.loadFromFile(std::string(path));
        }

        [[nodiscard]] bool loadFontFromFile(types::FontResource& out, std::string_view path) {
            return out.loadFromFile(std::string(path));
        }

        [[nodiscard]] bool loadSoundFromFile(types::SoundBufferResource& out,
                                             std::string_view path) {
            return out.loadFromFile(std::string(path));
        }

#if defined(SFML1_TESTS)
        TextureLoadFn gTextureLoadFn = &loadTextureFromFile;
        FontLoadFn gFontLoadFn = &loadFontFromFile;
        SoundLoadFn gSoundLoadFn = &loadSoundFromFile;
#endif

        [[nodiscard]] bool loadTexture(types::TextureResource& out, std::string_view path) {
#if defined(SFML1_TESTS)
            return gTextureLoadFn(out, path);
#else
            return loadTextureFromFile(out, path);
#endif
        }

        [[nodiscard]] bool loadFont(types::FontResource& out, std::string_view path) {
#if defined(SFML1_TESTS)
            return gFontLoadFn(out, path);
#else
            return loadFontFromFile(out, path);
#endif
        }

        [[nodiscard]] bool loadSound(types::SoundBufferResource& out, std::string_view path) {
#if defined(SFML1_TESTS)
            return gSoundLoadFn(out, path);
#else
            return loadSoundFromFile(out, path);
#endif
        }

#if defined(SFML1_PROFILE)
        // Генерация уникального ключа для дубликатов текстур в режиме стресс-теста.
        // Используем charconv для zero-allocation форматирования чисел.
        [[nodiscard]] std::string makeProfileStressTextureKey(ids::TextureID id) {
            using U = std::underlying_type_t<ids::TextureID>;
            static_assert(std::is_unsigned_v<U>, "TextureID underlying type must be unsigned.");

            constexpr std::size_t kPrefixLen = 3; // "psT"
            constexpr std::size_t kDigits =
                static_cast<std::size_t>(std::numeric_limits<U>::digits10) + 1;
            std::array<char, kPrefixLen + kDigits> buf{};

            char* out = buf.data();
            *out++ = 'p';
            *out++ = 's';
            *out++ = 'T';

            const auto raw = static_cast<U>(id);
            const auto [ptr, ec] = std::to_chars(out, buf.data() + buf.size(), raw);

            assert(ec == std::errc{} &&
                   "[makeProfileStressTextureKey] to_chars failed unexpectedly.");

            return std::string(buf.data(), ptr);
        }
#endif

        // ----------------------------------------------------------------------------------------
        // Generic helper для детерминированной предзагрузки ресурсов (LEGACY enum-ID).
        // ----------------------------------------------------------------------------------------
        template <typename EnumID, typename ConfigMap, typename LoadFunc>
        void preloadAllSorted(const ConfigMap& configs, LoadFunc&& loadFunc) {
            std::vector<EnumID> sortedIds;
            sortedIds.reserve(configs.size());

            for (const auto& [id, _] : configs) {
                sortedIds.push_back(id);
            }

            std::sort(sortedIds.begin(), sortedIds.end());

            for (const auto id : sortedIds) {
                loadFunc(id);
            }
        }

        [[nodiscard]] void panicKeyWorldNotInitialized(std::string_view where) {
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::{}] Key-world API вызван до initialize().", where);
        }

    } // namespace

    // --------------------------------------------------------------------------------------------
    // Bootstrapping (реестр ресурсов, LEGACY ResourcePaths)
    // --------------------------------------------------------------------------------------------

    void ResourceManager::loadRegistryFromJson(std::string_view filename) {
        // Boundary: validate on write. Повторный вызов — ошибка жизненного цикла движка.
        if (mRegistryLoaded) {
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::loadRegistryFromJson] Registry уже загружен; "
                      "повторный вызов запрещён. filename='{}'",
                      filename);
        }

        // Критические ошибки (I/O, битый JSON) обрабатываются внутри ResourcePaths (LOG_PANIC).
        paths::ResourcePaths::loadFromJSON(std::string(filename));
        mRegistryLoaded = true;
    }

    // --------------------------------------------------------------------------------------------
    // Bootstrapping (key-world, ResourceRegistry v1)
    // --------------------------------------------------------------------------------------------

    void ResourceManager::initialize(std::span<const ResourceSource> sources) {
        // Контракт: инициализация key-world выполняется ровно один раз.
        if (mInitialized) {
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::initialize] Key-world реестр уже инициализирован.");
        }

        mRegistry.loadFromSources(sources);

        mMissingTextureKey = mRegistry.missingTextureKey();
        mMissingFontKey = mRegistry.missingFontKey();

        if (!mMissingTextureKey.valid() || !mMissingFontKey.valid()) {
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::initialize] Некорректные fallback-ключи из реестра.");
        }

        mTextureCache.resize(mRegistry.textureCount());
        mFontCache.resize(mRegistry.fontCount());
        mSoundCache.resize(mRegistry.soundCount());

        mTextureState.assign(mTextureCache.size(), kStateNotAttempted);
        mFontState.assign(mFontCache.size(), kStateNotAttempted);
        mSoundState.assign(mSoundCache.size(), kStateNotAttempted);

        mInitialized = true;
    }

    const ResourceRegistry& ResourceManager::registry() const noexcept {
        if (!mInitialized) {
            panicKeyWorldNotInitialized("registry");
        }
        return mRegistry;
    }

    TextureKey ResourceManager::missingTextureKey() const noexcept {
        if (!mInitialized) {
            panicKeyWorldNotInitialized("missingTextureKey");
        }
        return mMissingTextureKey;
    }

    FontKey ResourceManager::missingFontKey() const noexcept {
        if (!mInitialized) {
            panicKeyWorldNotInitialized("missingFontKey");
        }
        return mMissingFontKey;
    }

    TextureKey ResourceManager::findTexture(std::string_view canonicalName) const {
        if (!mInitialized) {
            panicKeyWorldNotInitialized("findTexture");
        }
        return mRegistry.findTextureByName(canonicalName);
    }

    // --------------------------------------------------------------------------------------------
    // Key-based API (RuntimeKey32)
    // --------------------------------------------------------------------------------------------

    const types::TextureResource& ResourceManager::getTexture(TextureKey key) {
        if (!mInitialized) {
            panicKeyWorldNotInitialized("getTexture(TextureKey)");
        }

        if (!key.valid() || key.index() >= mTextureCache.size()) {
            return getTexture(mMissingTextureKey);
        }

        const std::uint32_t idx = key.index();
        const std::uint8_t state = mTextureState[idx];

        if (state == kStateLoaded) {
            const auto* cached = mTextureCache[idx].get();
            assert(cached != nullptr);
            return *cached;
        }

        const auto& entry = mRegistry.getTexture(key);

        // Контракт v1: path в Config legacy; source of truth = entry.path.
        auto resource = std::make_unique<types::TextureResource>();
        if (!loadTexture(*resource, entry.path)) {
            // Type A: любая ошибка загрузки текстуры — фатальна.
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::getTexture(TextureKey)] "
                      "Не удалось загрузить текстуру '{}'. path='{}'.",
                      entry.name, entry.path);
        }

        resource->setSmooth(entry.config.smooth);
        resource->setRepeated(entry.config.repeated);

        if (entry.config.generateMipmap) {
            if (!resource->generateMipmap()) {
                LOG_WARN(core::log::cat::Resources,
                         "[ResourceManager::getTexture(TextureKey)] "
                         "Не удалось сгенерировать mipmap: {}",
                         entry.path);
            }
        }

        mTextureCache[idx] = std::move(resource);
        mTextureState[idx] = kStateLoaded;
        return *mTextureCache[idx];
    }

    const types::FontResource& ResourceManager::getFont(FontKey key) {
        if (!mInitialized) {
            panicKeyWorldNotInitialized("getFont(FontKey)");
        }

        if (!key.valid() || key.index() >= mFontCache.size()) {
            return getFont(mMissingFontKey);
        }

        const std::uint32_t idx = key.index();
        const std::uint8_t state = mFontState[idx];

        if (state == kStateLoaded) {
            const auto* cached = mFontCache[idx].get();
            assert(cached != nullptr);
            return *cached;
        }

        const auto& entry = mRegistry.getFont(key);

        // Контракт v1: path в Config legacy; source of truth = entry.path.
        auto resource = std::make_unique<types::FontResource>();
        if (!loadFont(*resource, entry.path)) {
            // Type A: любая ошибка загрузки шрифта — фатальна.
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::getFont(FontKey)] "
                      "Не удалось загрузить шрифт '{}'. path='{}'.",
                      entry.name, entry.path);
        }

        mFontCache[idx] = std::move(resource);
        mFontState[idx] = kStateLoaded;
        return *mFontCache[idx];
    }

    const types::SoundBufferResource* ResourceManager::tryGetSound(SoundKey key) {
        if (!mInitialized) {
            panicKeyWorldNotInitialized("tryGetSound(SoundKey)");
        }

        if (!key.valid() || key.index() >= mSoundCache.size()) {
            return nullptr;
        }

        const std::uint32_t idx = key.index();
        const std::uint8_t state = mSoundState[idx];

        if (state == kStateLoaded) {
            const auto* cached = mSoundCache[idx].get();
            assert(cached != nullptr);
            return cached;
        }

        if (state == kStateFailed) {
            return nullptr;
        }

        const auto* entry = mRegistry.tryGetSound(key);
        if (entry == nullptr) {
            // Защита от рассинхрона ключа/реестра.
            mSoundState[idx] = kStateFailed;
            return nullptr;
        }

        // Контракт v1: path в Config legacy; source of truth = entry.path.
        auto resource = std::make_unique<types::SoundBufferResource>();
        if (!loadSound(*resource, entry->path)) {
            // Sounds — soft-fail: один WARN и больше не пробуем.
            mSoundState[idx] = kStateFailed;

            LOG_WARN(core::log::cat::Resources,
                     "[ResourceManager::tryGetSound(SoundKey)] "
                     "Не удалось загрузить звук '{}'. path='{}'.",
                     entry->name, entry->path);
            return nullptr;
        }

        mSoundCache[idx] = std::move(resource);
        mSoundState[idx] = kStateLoaded;
        return mSoundCache[idx].get();
    }

    // --------------------------------------------------------------------------------------------
    // Текстуры (LEGACY enum-ID)
    // --------------------------------------------------------------------------------------------

    const types::TextureResource& ResourceManager::getTexture(ids::TextureID id) {
#if defined(SFML1_PROFILE)
        // PROFILE stress mode:
        // При включении разрешаем "виртуальные" TextureID, которых нет в реестре.
        // Они становятся копиями sourceId в mDynamicTextures под уникальным ключом.
        // Важно: проверка должна быть ДО getTextureConfig(id), иначе ResourcePaths выдаст PANIC.
        if (mProfileStressTexturesEnabled && id != ids::TextureID::Unknown &&
            id != mProfileStressSourceTextureId && !paths::ResourcePaths::contains(id)) {

            const auto& srcCfg =
                paths::ResourcePaths::getTextureConfig(mProfileStressSourceTextureId);
            const std::string key = makeProfileStressTextureKey(id);

            auto [ptr, loadedNow] = mDynamicTextures.getOrLoad(key, srcCfg.path);
            if (!ptr) {
                LOG_PANIC(core::log::cat::Resources,
                          "[ResourceManager::getTexture(ProfileStress)] "
                          "Не удалось создать stress-текстуру key='{}' от source='{}'. path='{}'",
                          key, ids::toString(mProfileStressSourceTextureId), srcCfg.path);
            }
            if (loadedNow) {
                ptr->setSmooth(srcCfg.smooth);
                ptr->setRepeated(srcCfg.repeated);
                if (srcCfg.generateMipmap) {
                    if (!ptr->generateMipmap()) {
                        LOG_WARN(core::log::cat::Resources,
                                 "[ResourceManager::getTexture(ProfileStress)] "
                                 "Не удалось сгенерировать mipmap: {}",
                                 srcCfg.path);
                    }
                }
            }
            return *ptr;
        }
#endif

        const auto& cfg = paths::ResourcePaths::getTextureConfig(id);
        auto [ptr, loadedNow] = mTextures.getOrLoad(id, cfg.path);

        if (!ptr) {
            if (mHasMissingTextureFallback && id != mMissingTextureID) {
                LOG_WARN(core::log::cat::Resources,
                         "[ResourceManager::getTexture(TextureID)] "
                         "Не удалось загрузить текстуру для ID: {}. path='{}'. "
                         "Используется fallback: {}",
                         ids::toString(id), cfg.path, ids::toString(mMissingTextureID));
                return getTexture(mMissingTextureID);
            }
            if (mHasMissingTextureFallback && id == mMissingTextureID) {
                LOG_PANIC(core::log::cat::Resources,
                          "[ResourceManager::getTexture(TextureID)] "
                          "Fallback-текстура не загрузилась. ID: {}, path='{}'",
                          ids::toString(id), cfg.path);
            }
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::getTexture(TextureID)] "
                      "Не удалось загрузить текстуру. ID: {}, path='{}'. Fallback не задан.",
                      ids::toString(id), cfg.path);
        }

        if (loadedNow) {
            ptr->setSmooth(cfg.smooth);
            ptr->setRepeated(cfg.repeated);
            if (cfg.generateMipmap) {
                if (!ptr->generateMipmap()) {
                    LOG_WARN(core::log::cat::Resources,
                             "[ResourceManager::getTexture(TextureID)] "
                             "Не удалось сгенерировать mipmap: {}",
                             cfg.path);
                }
            }
        }

        return *ptr;
    }

    const types::TextureResource& ResourceManager::getTexture(const std::string& id) {
        return getTextureByPath(id);
    }

    const types::TextureResource& ResourceManager::getTextureByPath(const std::string& path) {
        auto [ptr, loadedNow] = mDynamicTextures.getOrLoad(path, path);

        if (!ptr) {
            if (mHasMissingTextureFallback) {
                LOG_WARN(core::log::cat::Resources,
                         "[ResourceManager::getTextureByPath] "
                         "Не удалось загрузить текстуру по пути: '{}'. Используется fallback: {}",
                         path, ids::toString(mMissingTextureID));
                return getTexture(mMissingTextureID);
            }
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::getTextureByPath] "
                      "Не удалось загрузить текстуру по пути: '{}'. Fallback не задан.",
                      path);
        }

        if (loadedNow) {
            // Политика по умолчанию для динамических текстур: smooth=true, repeated=false.
            ptr->setSmooth(true);
            ptr->setRepeated(false);
        }

        return *ptr;
    }

    // --------------------------------------------------------------------------------------------
    // Шрифты (LEGACY enum-ID)
    // --------------------------------------------------------------------------------------------

    const types::FontResource& ResourceManager::getFont(ids::FontID id) {
        const auto& cfg = paths::ResourcePaths::getFontConfig(id);
        auto [ptr, loadedNow] = mFonts.getOrLoad(id, cfg.path);
        (void) loadedNow;

        if (!ptr) {
            if (mHasMissingFontFallback && id != mMissingFontID) {
                LOG_WARN(core::log::cat::Resources,
                         "[ResourceManager::getFont(FontID)] "
                         "Не удалось загрузить шрифт для ID: {}. path='{}'. "
                         "Используется fallback: {}",
                         ids::toString(id), cfg.path, ids::toString(mMissingFontID));
                return getFont(mMissingFontID);
            }
            if (mHasMissingFontFallback && id == mMissingFontID) {
                LOG_PANIC(core::log::cat::Resources,
                          "[ResourceManager::getFont(FontID)] "
                          "Fallback-шрифт не загрузился. ID: {}, path='{}'",
                          ids::toString(id), cfg.path);
            }
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::getFont(FontID)] "
                      "Не удалось загрузить шрифт. ID: {}, path='{}'. Fallback не задан.",
                      ids::toString(id), cfg.path);
        }

        return *ptr;
    }

    const types::FontResource& ResourceManager::getFont(const std::string& id) {
        auto [ptr, loadedNow] = mDynamicFonts.getOrLoad(id, id);
        (void) loadedNow;

        if (!ptr) {
            if (mHasMissingFontFallback) {
                LOG_WARN(core::log::cat::Resources,
                         "[ResourceManager::getFont(std::string)] "
                         "Не удалось загрузить шрифт по пути: '{}'. Используется fallback: {}",
                         id, ids::toString(mMissingFontID));
                return getFont(mMissingFontID);
            }
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::getFont(std::string)] "
                      "Не удалось загрузить шрифт по пути: '{}'. Fallback не задан.",
                      id);
        }

        return *ptr;
    }

    // --------------------------------------------------------------------------------------------
    // Звуки (LEGACY enum-ID)
    // --------------------------------------------------------------------------------------------

    const types::SoundBufferResource& ResourceManager::getSound(ids::SoundID id) {
        if (auto* cached = mSounds.tryGet(id)) {
            return *cached;
        }

        if (!paths::ResourcePaths::contains(id)) {
            LOG_WARN(core::log::cat::Resources,
                     "[ResourceManager::getSound(SoundID)] "
                     "SoundID не зарегистрирован в реестре: {}. Возвращается пустой буфер.",
                     ids::toString(id));
            mSounds.insert(id, std::make_unique<types::SoundBufferResource>());
            return mSounds.get(id);
        }

        const auto& cfg = paths::ResourcePaths::getSoundConfig(id);
        auto [ptr, loadedNow] = mSounds.getOrLoad(id, cfg.path);
        (void) loadedNow;

        if (!ptr) {
            LOG_WARN(core::log::cat::Resources,
                     "[ResourceManager::getSound(SoundID)] "
                     "Не удалось загрузить звук для ID: {}. path='{}'. Возвращается пустой буфер.",
                     ids::toString(id), cfg.path);
            mSounds.insert(id, std::make_unique<types::SoundBufferResource>());
            return mSounds.get(id);
        }

        return *ptr;
    }

    const types::SoundBufferResource& ResourceManager::getSound(const std::string& id) {
        auto [ptr, loadedNow] = mDynamicSounds.getOrLoad(id, id);
        (void) loadedNow;

        if (!ptr) {
            LOG_WARN(core::log::cat::Resources,
                     "[ResourceManager::getSound(std::string)] "
                     "Не удалось загрузить звук по пути: '{}'. Возвращается пустой буфер.",
                     id);
            mDynamicSounds.insert(id, std::make_unique<types::SoundBufferResource>());
            return mDynamicSounds.get(id);
        }

        return *ptr;
    }

    // --------------------------------------------------------------------------------------------
    // Настройки Fallback (LEGACY)
    // --------------------------------------------------------------------------------------------

    void ResourceManager::setMissingTextureFallback(ids::TextureID id) {
        mMissingTextureID = id;
        mHasMissingTextureFallback = true;
        // Validate on write: fallback должен существовать.
        (void) getTexture(id);
    }

    void ResourceManager::setMissingFontFallback(ids::FontID id) {
        mMissingFontID = id;
        mHasMissingFontFallback = true;
        (void) getFont(id);
    }

    // --------------------------------------------------------------------------------------------
    // Метрики
    // --------------------------------------------------------------------------------------------

    ResourceManager::ResourceMetrics ResourceManager::getMetrics() const noexcept {
        return ResourceMetrics{
            .textureCount = mTextures.size(),
            .dynamicTextureCount = mDynamicTextures.size(),
            .fontCount = mFonts.size(),
            .dynamicFontCount = mDynamicFonts.size(),
            .soundCount = mSounds.size(),
            .dynamicSoundCount = mDynamicSounds.size(),
        };
    }

    // --------------------------------------------------------------------------------------------
    // API для предзагрузки ресурсов
    // --------------------------------------------------------------------------------------------

    void ResourceManager::preloadTexture(ids::TextureID id) {
        (void) getTexture(id);
    }
    void ResourceManager::preloadFont(ids::FontID id) {
        (void) getFont(id);
    }
    void ResourceManager::preloadSound(ids::SoundID id) {
        (void) getSound(id);
    }

    void ResourceManager::preloadAllTextures() {
        preloadAllSorted<ids::TextureID>(paths::ResourcePaths::getAllTextureConfigs(),
                                         [this](auto id) { (void) getTexture(id); });
    }

    void ResourceManager::preloadAllFonts() {
        preloadAllSorted<ids::FontID>(paths::ResourcePaths::getAllFontConfigs(),
                                      [this](auto id) { (void) getFont(id); });
    }

    void ResourceManager::preloadAllSounds() {
        preloadAllSorted<ids::SoundID>(paths::ResourcePaths::getAllSoundConfigs(),
                                       [this](auto id) { (void) getSound(id); });
    }

    void ResourceManager::preloadTexture(TextureKey key) {
        (void) getTexture(key);
    }
    void ResourceManager::preloadFont(FontKey key) {
        (void) getFont(key);
    }
    void ResourceManager::preloadSound(SoundKey key) {
        (void) tryGetSound(key);
    }

    void ResourceManager::preloadAllTexturesByRegistry() {
        if (!mInitialized) {
            panicKeyWorldNotInitialized("preloadAllTexturesByRegistry");
        }

        const auto count = static_cast<std::uint32_t>(mRegistry.textureCount());
        for (std::uint32_t i = 0; i < count; ++i) {
            preloadTexture(TextureKey::make(i));
        }
    }

    void ResourceManager::preloadAllFontsByRegistry() {
        if (!mInitialized) {
            panicKeyWorldNotInitialized("preloadAllFontsByRegistry");
        }

        const auto count = static_cast<std::uint32_t>(mRegistry.fontCount());
        for (std::uint32_t i = 0; i < count; ++i) {
            preloadFont(FontKey::make(i));
        }
    }

    void ResourceManager::preloadAllSoundsByRegistry() {
        if (!mInitialized) {
            panicKeyWorldNotInitialized("preloadAllSoundsByRegistry");
        }

        const auto count = static_cast<std::uint32_t>(mRegistry.soundCount());
        for (std::uint32_t i = 0; i < count; ++i) {
            preloadSound(SoundKey::make(i));
        }
    }

    // --------------------------------------------------------------------------------------------
    // Управление жизненным циклом ресурсов
    // --------------------------------------------------------------------------------------------

    void ResourceManager::unloadTexture(ids::TextureID id) noexcept {
        mTextures.unload(id);
    }
    void ResourceManager::unloadTexture(const std::string& id) noexcept {
        mDynamicTextures.unload(id);
    }

    void ResourceManager::unloadFont(ids::FontID id) noexcept {
        mFonts.unload(id);
    }
    void ResourceManager::unloadFont(const std::string& id) noexcept {
        mDynamicFonts.unload(id);
    }

    void ResourceManager::unloadSound(ids::SoundID id) noexcept {
        mSounds.unload(id);
    }
    void ResourceManager::unloadSound(const std::string& id) noexcept {
        mDynamicSounds.unload(id);
    }

    void ResourceManager::clearAll() noexcept {
        mTextures.clear();
        mFonts.clear();
        mSounds.clear();
        mDynamicTextures.clear();
        mDynamicFonts.clear();
        mDynamicSounds.clear();

        for (auto& entry : mTextureCache) {
            entry.reset();
        }
        for (auto& entry : mFontCache) {
            entry.reset();
        }
        for (auto& entry : mSoundCache) {
            entry.reset();
        }

        std::fill(mTextureState.begin(), mTextureState.end(), kStateNotAttempted);
        std::fill(mFontState.begin(), mFontState.end(), kStateNotAttempted);
        std::fill(mSoundState.begin(), mSoundState.end(), kStateNotAttempted);
    }

#if defined(SFML1_PROFILE)
    void ResourceManager::enableProfileStressTextureDuplication(ids::TextureID sourceId) noexcept {
        mProfileStressSourceTextureId = sourceId;
        mProfileStressTexturesEnabled = true;
    }

    void ResourceManager::disableProfileStressTextureDuplication() noexcept {
        mProfileStressTexturesEnabled = false;
    }
#endif

#if defined(SFML1_TESTS)
    namespace resource_manager::test {
        void setTextureLoadFn(TextureLoadFn fn) noexcept {
            gTextureLoadFn = fn;
        }
        void resetTextureLoadFn() noexcept {
            gTextureLoadFn = &loadTextureFromFile;
        }

        void setFontLoadFn(FontLoadFn fn) noexcept {
            gFontLoadFn = fn;
        }
        void resetFontLoadFn() noexcept {
            gFontLoadFn = &loadFontFromFile;
        }

        void setSoundLoadFn(SoundLoadFn fn) noexcept {
            gSoundLoadFn = fn;
        }
        void resetSoundLoadFn() noexcept {
            gSoundLoadFn = &loadSoundFromFile;
        }
    } // namespace resource_manager::test
#endif // defined(SFML1_TESTS)

} // namespace core::resources