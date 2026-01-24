#include "pch.h"

#include "core/resources/resource_manager.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <memory>
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

        [[nodiscard]] bool loadTextureFromFile(types::TextureResource& out,
                                               const std::filesystem::path& path,
                                               bool sRgb) {
            return out.loadFromFile(path, sRgb, sf::IntRect{});
        }

        [[nodiscard]] bool loadFontFromFile(types::FontResource& out,
                                            const std::filesystem::path& path) {
            return out.loadFromFile(path);
        }

        [[nodiscard]] bool loadSoundFromFile(types::SoundBufferResource& out,
                                             const std::filesystem::path& path) {
            return out.loadFromFile(path);
        }

#if defined(SFML1_TESTS)
        TextureLoadFn gTextureLoadFn = &loadTextureFromFile;
        FontLoadFn gFontLoadFn = &loadFontFromFile;
        SoundLoadFn gSoundLoadFn = &loadSoundFromFile;
#endif

        [[nodiscard]] bool loadTexture(types::TextureResource& out, 
                                       const std::filesystem::path& path,
                                       bool sRgb) {
#if defined(SFML1_TESTS)
            return gTextureLoadFn(out, path, sRgb);
#else
            return loadTextureFromFile(out, path, sRgb);
#endif
        }

        [[nodiscard]] bool loadFont(types::FontResource& out, 
                                    const std::filesystem::path& path) {
#if defined(SFML1_TESTS)
            return gFontLoadFn(out, path);
#else
            return loadFontFromFile(out, path);
#endif
        }

        [[nodiscard]] bool loadSound(types::SoundBufferResource& out, 
                                     const std::filesystem::path& path) {
#if defined(SFML1_TESTS)
            return gSoundLoadFn(out, path);
#else
            return loadSoundFromFile(out, path);
#endif
        }

        [[noreturn]] void panicKeyWorldNotInitialized(std::string_view where) {
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::{}] Key-world API вызван до initialize().",
                      where);
        }

        [[nodiscard]] std::filesystem::path toPath(std::string_view pathView) {
            // string_view -> path без промежуточного std::string.
            // path всё равно будет владеть строкой, но мы избегаем лишней аллокации/копии.
            return std::filesystem::path{pathView.begin(), pathView.end()};
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
        // Источник истины - entry.path.
        auto resource = std::make_unique<types::TextureResource>();
        const std::filesystem::path filePath = toPath(entry.path);

        if (!loadTexture(*resource, filePath, entry.config.srgb)) {
            // Type A: любая ошибка загрузки текстуры — фатальна.
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::getTexture(TextureKey)] "
                      "Не удалось загрузить текстуру '{}'. path='{}'.",
                      entry.name,
                      entry.path);
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
        // Источник истины - entry.path.
        auto resource = std::make_unique<types::FontResource>();
        const std::filesystem::path filePath = toPath(entry.path);

        if (!loadFont(*resource, filePath)) {
            // Type A: любая ошибка загрузки шрифта — фатальна.
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::getFont(FontKey)] "
                      "Не удалось загрузить шрифт '{}'. path='{}'.",
                      entry.name,
                      entry.path);
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
        // Защита от рассинхрона ключа/реестра.
        if (entry == nullptr) {
            mSoundState[idx] = kStateFailed;
            return nullptr;
        }
        // Источник истины - entry.path.
        auto resource = std::make_unique<types::SoundBufferResource>();
        const std::filesystem::path filePath = toPath(entry->path);

        if (!loadSound(*resource, filePath)) {
            // Sounds — soft-fail: один WARN и больше не пробуем.
            mSoundState[idx] = kStateFailed;

            LOG_WARN(core::log::cat::Resources,
                     "[ResourceManager::tryGetSound(SoundKey)] "
                     "Не удалось загрузить звук '{}'. path='{}'.",
                     entry->name,
                     entry->path);
            return nullptr;
        }

        mSoundCache[idx] = std::move(resource);
        mSoundState[idx] = kStateLoaded;
        return mSoundCache[idx].get();
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
            .fontCount = countLoaded(mFontState),
            .soundCount = countLoaded(mSoundState),
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
    // Key-world cache control
    // --------------------------------------------------------------------------------------------

    void ResourceManager::clearAll() noexcept {
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