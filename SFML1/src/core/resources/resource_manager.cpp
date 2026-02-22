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

        [[nodiscard]] std::filesystem::path toPath(std::string_view pathView) {
            // string_view -> path без промежуточного std::string.
            // path всё равно будет владеть строкой, но мы избегаем лишней аллокации/копии.
            return std::filesystem::path{pathView.begin(), pathView.end()};
        }

        // ----------------------------------------------------------------------------------------
        // Продакшн-загрузчики с диска (default implementations).
        // Используются, если соответствующее поле Loaders пусто.
        // ----------------------------------------------------------------------------------------

        [[nodiscard]] bool defaultLoadTexture(types::TextureResource& out,
                                              std::string_view path,
                                              bool sRgb) {
            return out.loadFromFile(toPath(path), sRgb, sf::IntRect{});
        }

        [[nodiscard]] bool defaultLoadFont(types::FontResource& out, std::string_view path) {
            return out.loadFromFile(toPath(path));
        }

        [[nodiscard]] bool defaultLoadSound(types::SoundBufferResource& out,
                                            std::string_view path) {
            return out.loadFromFile(toPath(path));
        }

        // ----------------------------------------------------------------------------------------
        // Dispatch wrappers — вызываем инъецированный загрузчик или fallback на default.
        //
        // Примечание по стоимости:
        //  - В продакшне при Loaders{} здесь одна проверка на empty и прямой вызов default.
        //  - При инъекции тестового загрузчика — вызов через std::function (SBO, без heap).
        //  - Это НЕ hot-loop; стоимость тонет в файловом I/O.
        // ----------------------------------------------------------------------------------------

        [[nodiscard]] bool doLoadTexture(const ResourceManager::Loaders& loaders,
                                         types::TextureResource& out,
                                         std::string_view path,
                                         bool sRgb) {
            if (loaders.texture) {
                return loaders.texture(out, path, sRgb);
            }
            return defaultLoadTexture(out, path, sRgb);
        }

        [[nodiscard]] bool doLoadFont(const ResourceManager::Loaders& loaders,
                                      types::FontResource& out,
                                      std::string_view path) {
            if (loaders.font) {
                return loaders.font(out, path);
            }
            return defaultLoadFont(out, path);
        }

        [[nodiscard]] bool doLoadSound(const ResourceManager::Loaders& loaders,
                                       types::SoundBufferResource& out,
                                       std::string_view path) {
            if (loaders.sound) {
                return loaders.sound(out, path);
            }
            return defaultLoadSound(out, path);
        }

        // ----------------------------------------------------------------------------------------
        // Panic helpers
        // ----------------------------------------------------------------------------------------

        [[noreturn]] void panicNotInitialized(std::string_view where) {
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::{}] Key-world API вызван до initialize().", where);
        }

        [[noreturn]] void panicLazyLoadForbidden(std::string_view where,
                                                 std::string_view name,
                                                 std::string_view path) {
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::{}] Lazy-load запрещён после init/preload. "
                      "Ресурс не resident (забыли preload?). name='{}' path='{}'.",
                      where, name, path);
        }

    } // namespace

    // --------------------------------------------------------------------------------------------
    // Bootstrapping
    // --------------------------------------------------------------------------------------------

    void ResourceManager::bumpCacheGeneration() noexcept {
        ++mCacheGeneration;
        if (mCacheGeneration == 0u) {
            mCacheGeneration = 1u;
        }
    }

    void ResourceManager::initialize(std::span<const ResourceSource> sources, Loaders loaders) {
        if (mInitialized) {
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::initialize] Key-world реестр уже инициализирован.");
        }

        mLoaders = std::move(loaders);
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

        mInitialized = true;

        if (mMissingTextureKey.index() >= mTextureCache.size()) {
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::initialize] missingTextureKey index out of range "
                      "(idx={}, count={})",
                      mMissingTextureKey.index(), mTextureCache.size());
        }
        if (mMissingFontKey.index() >= mFontCache.size()) {
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::initialize] missingFontKey index out of range "
                      "(idx={}, count={})",
                      mMissingFontKey.index(), mFontCache.size());
        }

        // КРИТИЧНО: fallback ресурсы должны быть resident всегда.
        (void) getTexture(mMissingTextureKey);
        (void) getFont(mMissingFontKey);

        bumpCacheGeneration();
    }

    void ResourceManager::setIoForbidden(bool enabled) noexcept {
        mIoForbidden = enabled;
    }

    std::uint32_t ResourceManager::cacheGeneration() const noexcept {
        if (!mInitialized) {
            panicNotInitialized("cacheGeneration");
        }
        return mCacheGeneration;
    }

    const ResourceRegistry& ResourceManager::registry() const noexcept {
        if (!mInitialized) {
            panicNotInitialized("registry");
        }
        return mRegistry;
    }

    TextureKey ResourceManager::missingTextureKey() const noexcept {
        if (!mInitialized) {
            panicNotInitialized("missingTextureKey");
        }
        return mMissingTextureKey;
    }

    FontKey ResourceManager::missingFontKey() const noexcept {
        if (!mInitialized) {
            panicNotInitialized("missingFontKey");
        }
        return mMissingFontKey;
    }

    TextureKey ResourceManager::findTexture(std::string_view canonicalName) const {
        if (!mInitialized) {
            panicNotInitialized("findTexture");
        }
        return mRegistry.findTextureByName(canonicalName);
    }

    FontKey ResourceManager::findFont(std::string_view canonicalName) const {
        if (!mInitialized) {
            panicNotInitialized("findFont");
        }
        return mRegistry.findFontByName(canonicalName);
    }

    // --------------------------------------------------------------------------------------------
    // Key-based API (RuntimeKey32)
    // --------------------------------------------------------------------------------------------

    const types::TextureResource& ResourceManager::getTexture(TextureKey key) {
        if (!mInitialized) {
            panicNotInitialized("getTexture(TextureKey)");
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

        auto resource = std::make_unique<types::TextureResource>();

        if (!doLoadTexture(mLoaders, *resource, entry.path, entry.config.srgb)) {
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
        mTextureState[idx] = ResourceState::Resident;
        return *mTextureCache[idx];
    }

    const types::FontResource& ResourceManager::getFont(FontKey key) {
        if (!mInitialized) {
            panicNotInitialized("getFont(FontKey)");
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

        auto resource = std::make_unique<types::FontResource>();

        if (!doLoadFont(mLoaders, *resource, entry.path)) {
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::getFont(FontKey)] "
                      "Не удалось загрузить шрифт '{}'. path='{}'.",
                      entry.name, entry.path);
        }

        mFontCache[idx] = std::move(resource);
        mFontState[idx] = ResourceState::Resident;
        return *mFontCache[idx];
    }

    const types::SoundBufferResource* ResourceManager::tryGetSound(SoundKey key) {
        if (!mInitialized) {
            panicNotInitialized("tryGetSound(SoundKey)");
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
        if (entry == nullptr) {
            mSoundState[idx] = ResourceState::Failed;
            return nullptr;
        }

        if (mIoForbidden) {
            panicLazyLoadForbidden("tryGetSound(SoundKey)", entry->name, entry->path);
        }

        auto resource = std::make_unique<types::SoundBufferResource>();

        if (!doLoadSound(mLoaders, *resource, entry->path)) {
            mSoundState[idx] = ResourceState::Failed;
            LOG_WARN(core::log::cat::Resources,
                     "[ResourceManager::tryGetSound(SoundKey)] "
                     "Не удалось загрузить звук '{}'. path='{}'.",
                     entry->name, entry->path);
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
            panicNotInitialized("expectTextureResident(TextureKey)");
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
                      entry.name, entry.path);
        }

        const auto* cached = mTextureCache[idx].get();
        assert(cached != nullptr);
        return *cached;
    }

    const types::FontResource& ResourceManager::expectFontResident(FontKey key) const {
        if (!mInitialized) {
            panicNotInitialized("expectFontResident(FontKey)");
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
                      entry.name, entry.path);
        }

        const auto* cached = mFontCache[idx].get();
        assert(cached != nullptr);
        return *cached;
    }

    const types::SoundBufferResource*
    ResourceManager::tryGetSoundResident(SoundKey key) const noexcept {
        if (!mInitialized) {
            panicNotInitialized("tryGetSoundResident(SoundKey)");
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
            .fontCount    = countResident(mFontState),
            .soundCount   = countResident(mSoundState),
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
            panicNotInitialized("preloadAllTexturesByRegistry");
        }

        const auto count = static_cast<std::uint32_t>(mRegistry.textureCount());
        for (std::uint32_t i = 0; i < count; ++i) {
            preloadTexture(TextureKey::make(i));
        }
    }

    void ResourceManager::preloadAllFontsByRegistry() {
        if (!mInitialized) {
            panicNotInitialized("preloadAllFontsByRegistry");
        }

        const auto count = static_cast<std::uint32_t>(mRegistry.fontCount());
        for (std::uint32_t i = 0; i < count; ++i) {
            preloadFont(FontKey::make(i));
        }
    }

    void ResourceManager::preloadAllSoundsByRegistry() {
        if (!mInitialized) {
            panicNotInitialized("preloadAllSoundsByRegistry");
        }

        const auto count = static_cast<std::uint32_t>(mRegistry.soundCount());
        for (std::uint32_t i = 0; i < count; ++i) {
            preloadSound(SoundKey::make(i));
        }
    }

    // --------------------------------------------------------------------------------------------
    // Cache control
    // --------------------------------------------------------------------------------------------

    void ResourceManager::clearAll() noexcept {
        if (!mInitialized) {
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::clearAll] Key-world API вызван до initialize().");
        }

        if (!mMissingTextureKey.valid()) {
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::clearAll] invalid missingTextureKey "
                      "(invariant broken).");
        }
        if (!mMissingFontKey.valid()) {
            LOG_PANIC(core::log::cat::Resources,
                      "[ResourceManager::clearAll] invalid missingFontKey "
                      "(invariant broken).");
        }

        const std::uint32_t missingTexIdx  = mMissingTextureKey.index();
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

} // namespace core::resources