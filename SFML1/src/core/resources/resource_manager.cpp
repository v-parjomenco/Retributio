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

        [[noreturn]] void panicLazyLoadForbidden(std::string_view where,
                                                 std::string_view name,
                                                 std::string_view path) {
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::{}] Lazy-load запрещён после init/preload. "
                      "Ресурс не resident (забыли preload?). name='{}' path='{}'.",
                      where, name, path);
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

    void ResourceManager::bumpCacheGeneration() noexcept {
        ++mCacheGeneration;
        // 0 оставляем как "неожиданное wrap-around"; но чтобы сравнение было стабильным,
        // не допускаем 0 как валидное поколение.
        if (mCacheGeneration == 0u) {
            mCacheGeneration = 1u;
        }
    }

    void ResourceManager::initialize(std::span<const ResourceSource> sources) {
        // Контракт: инициализация key-world выполняется ровно один раз.
        if (mInitialized) {
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::initialize] Key-world реестр уже инициализирован.");
        }

        // На фазе инициализации lazy-load разрешён, даже если запрет был ранее включён.
        mIoForbidden = false;

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

        mTextureState.assign(mTextureCache.size(), ResourceState::NotLoaded);
        mFontState.assign(mFontCache.size(), ResourceState::NotLoaded);
        mSoundState.assign(mSoundCache.size(), ResourceState::NotLoaded);

        mInitialized = true; // нужно для preload fallback через публичный API

        // validate-on-write: missing-ключи обязаны быть в диапазоне.
        if (mMissingTextureKey.index() >= mTextureCache.size()) {
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::initialize] missingTextureKey index out of range "
                      "(idx={}, count={})",
                      mMissingTextureKey.index(),
                      mTextureCache.size());
        }
        if (mMissingFontKey.index() >= mFontCache.size()) {
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::initialize] missingFontKey index out of range "
                      "(idx={}, count={})",
                      mMissingFontKey.index(),
                      mFontCache.size());
        }

        // КРИТИЧНО: fallback ресурсы должны быть resident всегда.
        (void) getTexture(mMissingTextureKey);
        (void) getFont(mMissingFontKey);

        bumpCacheGeneration();

    }

    void ResourceManager::setIoForbidden(const bool enabled) noexcept {
        mIoForbidden = enabled;
    }

    std::uint32_t ResourceManager::cacheGeneration() const noexcept {
        if (!mInitialized) {
            panicKeyWorldNotInitialized("cacheGeneration");
        }
        return mCacheGeneration;
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
        const ResourceState state = mTextureState[idx];

        if (state == ResourceState::Resident) {
            const auto* cached = mTextureCache[idx].get();
            assert(cached != nullptr);
            return *cached;
        }

        const auto& entry = mRegistry.getTexture(key);

        if (mIoForbidden) {
            panicLazyLoadForbidden("getTexture(TextureKey)", entry.name, entry.path);
        }

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
        mTextureState[idx] = ResourceState::Resident;
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
        const ResourceState state = mFontState[idx];

        if (state == ResourceState::Resident) {
            const auto* cached = mFontCache[idx].get();
            assert(cached != nullptr);
            return *cached;
        }

        const auto& entry = mRegistry.getFont(key);

        if (mIoForbidden) {
            panicLazyLoadForbidden("getFont(FontKey)", entry.name, entry.path);
        }

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
        mFontState[idx] = ResourceState::Resident;
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
        const ResourceState state = mSoundState[idx];

        if (state == ResourceState::Resident) {
            const auto* cached = mSoundCache[idx].get();
            assert(cached != nullptr);
            return cached;
        }

        if (state == ResourceState::Failed) {
            return nullptr;
        }

        const auto* entry = mRegistry.tryGetSound(key);
        // Защита от рассинхрона ключа/реестра.
        if (entry == nullptr) {
            mSoundState[idx] = ResourceState::Failed;
            return nullptr;
        }

        if (mIoForbidden) {
            // Sounds всё равно soft-fail, но I/O после init/preload запрещён как класс.
            panicLazyLoadForbidden("tryGetSound(SoundKey)", entry->name, entry->path);
        }

        // Источник истины - entry.path.
        auto resource = std::make_unique<types::SoundBufferResource>();
        const std::filesystem::path filePath = toPath(entry->path);

        if (!loadSound(*resource, filePath)) {
            // Sounds — soft-fail: один WARN и больше не пробуем.
            mSoundState[idx] = ResourceState::Failed;

            LOG_WARN(core::log::cat::Resources,
                     "[ResourceManager::tryGetSound(SoundKey)] "
                     "Не удалось загрузить звук '{}'. path='{}'.",
                     entry->name,
                     entry->path);
            return nullptr;
        }

        mSoundCache[idx] = std::move(resource);
        mSoundState[idx] = ResourceState::Resident;
        return mSoundCache[idx].get();
    }

    // --------------------------------------------------------------------------------------------
    // Resident-only API (NO I/O)
    // --------------------------------------------------------------------------------------------

    const types::TextureResource& ResourceManager::expectTextureResident(TextureKey key) const {
        if (!mInitialized) {
            panicKeyWorldNotInitialized("expectTextureResident(TextureKey)");
        }

        if (!key.valid() || key.index() >= mTextureCache.size()) {
            key = mMissingTextureKey;
        }

        const std::uint32_t idx = key.index();
        const ResourceState state = mTextureState[idx];
        if (state != ResourceState::Resident) {
            const auto& entry = mRegistry.getTexture(key);
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::expectTextureResident(TextureKey)] "
                      "Texture is not resident (forgot preload?). name='{}' path='{}'.",
                      entry.name,
                      entry.path);
        }

        const auto* cached = mTextureCache[idx].get();
        assert(cached != nullptr);
        return *cached;
    }

    const types::FontResource& ResourceManager::expectFontResident(FontKey key) const {
        if (!mInitialized) {
            panicKeyWorldNotInitialized("expectFontResident(FontKey)");
        }

        if (!key.valid() || key.index() >= mFontCache.size()) {
            key = mMissingFontKey;
        }

        const std::uint32_t idx = key.index();
        const ResourceState state = mFontState[idx];
        if (state != ResourceState::Resident) {
            const auto& entry = mRegistry.getFont(key);
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::expectFontResident(FontKey)] "
                      "Font is not resident (forgot preload?). name='{}' path='{}'.",
                      entry.name,
                      entry.path);
        }

        const auto* cached = mFontCache[idx].get();
        assert(cached != nullptr);
        return *cached;
    }

    const types::SoundBufferResource* ResourceManager::tryGetSoundResident(SoundKey key) const noexcept {
        if (!mInitialized) {
            panicKeyWorldNotInitialized("tryGetSoundResident(SoundKey)");
        }

        if (!key.valid() || key.index() >= mSoundCache.size()) {
            return nullptr;
        }

        const std::uint32_t idx = key.index();
        if (mSoundState[idx] != ResourceState::Resident) {
            return nullptr;
        }

        return mSoundCache[idx].get();
    }

    // --------------------------------------------------------------------------------------------
    // Метрики
    // --------------------------------------------------------------------------------------------

    ResourceManager::ResourceMetrics ResourceManager::getMetrics() const noexcept {
        const auto countResident = 
            [](const std::vector<ResourceState>& states) noexcept -> std::size_t {
            return static_cast<std::size_t>(
                std::count(states.begin(), states.end(), ResourceState::Resident));
        };

        return ResourceMetrics{
            .textureCount = countResident(mTextureState),
            .fontCount = countResident(mFontState),
            .soundCount = countResident(mSoundState),
        };
    }

    // --------------------------------------------------------------------------------------------
    // Batch preload
    // --------------------------------------------------------------------------------------------

    void ResourceManager::preloadTextures(std::span<const TextureKey> keys) {
        for (const TextureKey k : keys) {
            preloadTexture(k);
        }
    }

    void ResourceManager::preloadFonts(std::span<const FontKey> keys) {
        for (const FontKey k : keys) {
            preloadFont(k);
        }
    }

    void ResourceManager::preloadSounds(std::span<const SoundKey> keys) {
        for (const SoundKey k : keys) {
            preloadSound(k);
        }
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
    if (!mInitialized) {
        LOG_PANIC(core::log::cat::Resources,
                  "[ResourceManager::clearAll] Key-world API вызван до initialize().");
    }

    // Инвариант: missing texture/font обязаны быть resident в любой момент после initialize().
    if (!mMissingTextureKey.valid()) {
        LOG_PANIC(core::log::cat::Resources,
                  "[ResourceManager::clearAll] invalid missingTextureKey (invariant broken).");
    }
    if (!mMissingFontKey.valid()) {
        LOG_PANIC(core::log::cat::Resources,
                  "[ResourceManager::clearAll] invalid missingFontKey (invariant broken).");
    }

    const std::uint32_t missingTexIdx = mMissingTextureKey.index();
    const std::uint32_t missingFontIdx = mMissingFontKey.index();

    if (missingTexIdx >= mTextureCache.size() || missingTexIdx >= mTextureState.size()) {
        LOG_PANIC(core::log::cat::Resources,
                  "[ResourceManager::clearAll] missingTextureKey index out of range "
                  "(idx={}, texCache={}, texState={}).",
                  missingTexIdx, mTextureCache.size(), mTextureState.size());
    }
    if (missingFontIdx >= mFontCache.size() || missingFontIdx >= mFontState.size()) {
        LOG_PANIC(core::log::cat::Resources,
                  "[ResourceManager::clearAll] missingFontKey index out of range "
                  "(idx={}, fontCache={}, fontState={}).",
                  missingFontIdx, mFontCache.size(), mFontState.size());
    }

    const bool missingTexResident =
        (mTextureState[missingTexIdx] == ResourceState::Resident) &&
        (mTextureCache[missingTexIdx] != nullptr);

    const bool missingFontResident =
        (mFontState[missingFontIdx] == ResourceState::Resident) &&
        (mFontCache[missingFontIdx] != nullptr);

    if (!missingTexResident) {
        LOG_PANIC(core::log::cat::Resources,
                  "[ResourceManager::clearAll] missing texture is not resident "
                  "(invariant broken).");
    }
    if (!missingFontResident) {
        LOG_PANIC(core::log::cat::Resources,
                  "[ResourceManager::clearAll] missing font is not resident "
                  "(invariant broken).");
    }

    // Очищаем всё, кроме missing (они должны оставаться resident).
    for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(mTextureCache.size()); ++i) {
        if (i == missingTexIdx) {
            continue;
        }
        mTextureCache[i].reset();
        mTextureState[i] = ResourceState::NotLoaded;
    }

    for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(mFontCache.size()); ++i) {
        if (i == missingFontIdx) {
            continue;
        }
        mFontCache[i].reset();
        mFontState[i] = ResourceState::NotLoaded;
    }

    for (auto& entry : mSoundCache) {
        entry.reset();
    }
    std::fill(mSoundState.begin(), mSoundState.end(), ResourceState::NotLoaded);

    bumpCacheGeneration();
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