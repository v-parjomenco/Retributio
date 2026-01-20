#include "pch.h"

#include "core/resources/resource_manager.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "core/log/log_macros.h"

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

        [[noreturn]] void panicKeyWorldNotInitialized(std::string_view where) {
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::{}] Key-world API вызван до initialize().", where);
        }

    } // namespace

    // --------------------------------------------------------------------------------------------
    // Bootstrapping (key-world, ResourceRegistry)
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

    FontKey ResourceManager::findFont(std::string_view canonicalName) const {
        if (!mInitialized) {
            panicKeyWorldNotInitialized("findFont");
        }
        return mRegistry.findFontByName(canonicalName);
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

    const types::TextureResource& ResourceManager::getTexture(const std::string& id) {
        return getTextureByPath(id);
    }

    const types::TextureResource& ResourceManager::getTextureByPath(const std::string& path) {
        auto [ptr, loadedNow] = mDynamicTextures.getOrLoad(path, path);

        if (!ptr) {
            if (mInitialized) {
                LOG_WARN(core::log::cat::Resources,
                         "[ResourceManager::getTextureByPath] "
                         "Не удалось загрузить текстуру по пути: '{}'. Используется fallback "
                         "key.index(): {}",
                         path, mMissingTextureKey.index());
                return getTexture(mMissingTextureKey);
            }
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::getTextureByPath] "
                      "Не удалось загрузить текстуру по пути: '{}'. Fallback недоступен "
                      "(key-world не инициализирован).",
                      path);
        }

        if (loadedNow) {
            // Политика по умолчанию для динамических текстур: smooth=true, repeated=false.
            ptr->setSmooth(true);
            ptr->setRepeated(false);
        }

        return *ptr;
    }

    const types::FontResource& ResourceManager::getFont(const std::string& id) {
        auto [ptr, loadedNow] = mDynamicFonts.getOrLoad(id, id);
        (void) loadedNow;

        if (!ptr) {
            if (mInitialized) {
                LOG_WARN(core::log::cat::Resources,
                         "[ResourceManager::getFont(std::string)] "
                         "Не удалось загрузить шрифт по пути: '{}'. Используется fallback "
                         "key.index(): {}",
                         id, mMissingFontKey.index());
                return getFont(mMissingFontKey);
            }
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::getFont(std::string)] "
                      "Не удалось загрузить шрифт по пути: '{}'. Fallback недоступен "
                      "(key-world не инициализирован).",
                      id);
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
    // Метрики
    // --------------------------------------------------------------------------------------------

    ResourceManager::ResourceMetrics ResourceManager::getMetrics() const noexcept {
        const auto countLoaded = [](const std::vector<std::uint8_t>& states) -> std::size_t {
            return static_cast<std::size_t>(std::count(states.begin(), states.end(), kStateLoaded));
        };

        return ResourceMetrics{
            .textureCount = countLoaded(mTextureState),
            .dynamicTextureCount = mDynamicTextures.size(),
            .fontCount = countLoaded(mFontState),
            .dynamicFontCount = mDynamicFonts.size(),
            .soundCount = countLoaded(mSoundState),
            .dynamicSoundCount = mDynamicSounds.size(),
        };
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

    void ResourceManager::unloadTexture(const std::string& id) noexcept {
        mDynamicTextures.unload(id);
    }

    void ResourceManager::unloadFont(const std::string& id) noexcept {
        mDynamicFonts.unload(id);
    }

    void ResourceManager::unloadSound(const std::string& id) noexcept {
        mDynamicSounds.unload(id);
    }

    void ResourceManager::clearAll() noexcept {
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