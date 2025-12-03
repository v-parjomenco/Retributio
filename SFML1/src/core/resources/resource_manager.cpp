#include "pch.h"

#include "core/resources/resource_manager.h"
#include <cassert>
#include <stdexcept>

#include "core/utils/message.h"

namespace core::resources {

    namespace {
            namespace message = core::utils::message;

            using core::resources::config::FontResourceConfig;
            using core::resources::config::SoundResourceConfig;
            using core::resources::config::TextureResourceConfig;

            template <typename Cache, typename Key>
            const types::TextureResource&
            ensureTextureLoadedWithConfig(Cache& cache, const Key& key,
                                          const TextureResourceConfig& config) {
                if (!cache.contains(key)) {
                    [[maybe_unused]] const bool wasLoaded = cache.load(key, config.path);
                    assert(wasLoaded &&
                           "[ResourceManager] ResourceHolder::load(...) returned false "
                           "while !contains(key).");

                    auto& textureResource = cache.get(key);
                    textureResource.setSmooth(config.smooth);
                    textureResource.setRepeated(config.repeated);

                    if (config.generateMipmap) {
                        if (!textureResource.generateMipmap()) {
                            message::logDebug("[ResourceManager::ensureTextureLoadedWithConfig]\n"
                                              "Не удалось сгенерировать mipmap для текстуры: " +
                                              config.path);
                        }
                    }
                }

                return cache.get(key);
            }

            template <typename Cache, typename Key>
            const types::FontResource& ensureFontLoadedWithConfig(Cache& cache, const Key& key,
                                                                  const FontResourceConfig& config) {
                if (!cache.contains(key)) {
                    [[maybe_unused]] const bool wasLoaded = cache.load(key, config.path);
                    assert(wasLoaded &&
                           "[ResourceManager] ResourceHolder::load(...) returned false "
                           "while !contains(key).");
                }

                return cache.get(key);
            }

            template <typename Cache, typename Key>
            const types::SoundBufferResource&
            ensureSoundLoadedWithConfig(Cache& cache, const Key& key,
                                        const SoundResourceConfig& config) {
                if (!cache.contains(key)) {
                    [[maybe_unused]] const bool wasLoaded = cache.load(key, config.path);
                    assert(wasLoaded && "[ResourceManager] ResourceHolder::load(...) returned false "
                                        "while !contains(key).");
                }

                return cache.get(key);
            }
        } // namespace

    // --------------------------------------------------------------------------------------------
    // Текстуры
    // --------------------------------------------------------------------------------------------

    const types::TextureResource& ResourceManager::getTexture(ids::TextureID id) {
        try {
            const auto& textureConfig = paths::ResourcePaths::getTextureConfig(id);
            return ensureTextureLoadedWithConfig(mTextures, id, textureConfig);
        } catch (const std::exception& exception) {
            // Если задан fallback и это не сам fallback-ID — пробуем вернуть его.
            if (mHasMissingTextureFallback && id != mMissingTextureID) {
                message::logDebug(std::string("[ResourceManager::getTexture(TextureID)]\n"
                                              "Не удалось получить текстуру для ID: ") +
                                              std::string(ids::toString(id)) +
                                              ". Используем fallback-текстуру: " +
                                              std::string(ids::toString(mMissingTextureID)) +
                                              ". Ошибка: " + exception.what());

                return getTexture(mMissingTextureID);
            }

            throw; // Fallback не задан — пусть ошибка поднимается выше.
        }
    }

    const types::TextureResource& ResourceManager::getTexture(const std::string& id) {
        try {
            TextureResourceConfig config{};
            config.path = id;
            config.smooth = true;
            config.repeated = false;
            config.generateMipmap = false;

            return ensureTextureLoadedWithConfig(mDynamicTextures, id, config);
        } catch (const std::exception& exception) {
            if (mHasMissingTextureFallback) {
                message::logDebug(std::string("[ResourceManager::getTexture(std::string)]\n"
                                              "Не удалось получить текстуру по строковому ID: ") +
                                              id + ". Используем fallback-текстуру: " +
                                              std::string(ids::toString(mMissingTextureID)) +
                                              ". Ошибка: " + exception.what());

                return getTexture(mMissingTextureID);
            }

            throw;
        }
    }

    const types::TextureResource& ResourceManager::getTextureByPath(const std::string& path) {
        TextureResourceConfig config{};
        config.path = path;
        config.smooth = true;
        config.repeated = false;
        config.generateMipmap = false;

        try {
            return ensureTextureLoadedWithConfig(mDynamicTextures, path, config);
        } catch (const std::exception& exception) {
            if (mHasMissingTextureFallback) {
                message::logDebug(std::string("[ResourceManager::getTextureByPath]\n"
                                              "Не удалось загрузить текстуру по пути: ") +
                                              path + ". Используем fallback-текстуру: " +
                                              std::string(ids::toString(mMissingTextureID)) +
                                              ". Ошибка: " + exception.what());

                return getTexture(mMissingTextureID);
            }

            throw;
        }
    }

    // --------------------------------------------------------------------------------------------
    // Шрифты
    // --------------------------------------------------------------------------------------------

    const types::FontResource& ResourceManager::getFont(ids::FontID id) {
        try {
            const auto& fontConfig = paths::ResourcePaths::getFontConfig(id);
            return ensureFontLoadedWithConfig(mFonts, id, fontConfig);
        } catch (const std::exception& exception) {
            if (mHasMissingFontFallback && id != mMissingFontID) {
                message::logDebug(std::string("[ResourceManager::getFont(FontID)]\n"
                                              "Не удалось получить шрифт для ID: ") +
                                              std::string(ids::toString(id)) + 
                                              ". Используем fallback-шрифт: " +
                                              std::string(ids::toString(mMissingFontID)) +
                                              ". Ошибка: " + exception.what());

                return getFont(mMissingFontID);
            }

            throw;
        }
    }

    const types::FontResource& ResourceManager::getFont(const std::string& id) {
        try {
            FontResourceConfig config{};
            config.path = id;

            return ensureFontLoadedWithConfig(mDynamicFonts, id, config);
        } catch (const std::exception& exception) {
            if (mHasMissingFontFallback) {
                message::logDebug(std::string("[ResourceManager::getFont(std::string)]\n"
                                              "Не удалось получить шрифт по строковому ID: ") +
                                              id + ". Используем fallback-шрифт: " +
                                              std::string(ids::toString(mMissingFontID)) +
                                              ". Ошибка: " + exception.what());

                return getFont(mMissingFontID);
            }

            throw;
        }
    }

    // --------------------------------------------------------------------------------------------
    // Звуки
    // --------------------------------------------------------------------------------------------

    const types::SoundBufferResource& ResourceManager::getSound(ids::SoundID id) {
        try {
            const auto& soundConfig = paths::ResourcePaths::getSoundConfig(id);
            return ensureSoundLoadedWithConfig(mSounds, id, soundConfig);
        } catch (const std::exception& exception) {
            if (mHasMissingSoundFallback && id != mMissingSoundID) {
                message::logDebug(std::string("[ResourceManager::getSound(SoundID)]\n"
                                              "Не удалось получить звук для ID: ") +
                                              std::string(ids::toString(id)) + 
                                              ". Используем fallback-звук: " +
                                              std::string(ids::toString(mMissingSoundID)) +
                                              ". Ошибка: " + exception.what());

                return getSound(mMissingSoundID);
            }

            throw;
        }
    }

    const types::SoundBufferResource& ResourceManager::getSound(const std::string& id) {
        try {
            SoundResourceConfig config{};
            config.path = id;

            return ensureSoundLoadedWithConfig(mDynamicSounds, id, config);
        } catch (const std::exception& exception) {
            if (mHasMissingSoundFallback) {
                message::logDebug(std::string("[ResourceManager::getSound(std::string)]\n"
                                              "Не удалось получить звук по строковому ID: ") +
                                              id + ". Используем fallback-звук: " +
                                              std::string(ids::toString(mMissingSoundID)) +
                                              ". Ошибка: " + exception.what());

                return getSound(mMissingSoundID);
            }

            throw;
        }
    }

    // --------------------------------------------------------------------------------------------
    // Fallback-ресурсы
    // --------------------------------------------------------------------------------------------

    void ResourceManager::setMissingTextureFallback(ids::TextureID id) {
        mMissingTextureID = id;
        mHasMissingTextureFallback = true;

        // Прогреваем/валидируем fallback-текстуру сразу.
        (void) getTexture(id);
    }

    void ResourceManager::setMissingFontFallback(ids::FontID id) {
        mMissingFontID = id;
        mHasMissingFontFallback = true;

        (void) getFont(id);
    }

    void ResourceManager::setMissingSoundFallback(ids::SoundID id) {
        mMissingSoundID = id;
        mHasMissingSoundFallback = true;

        (void) getSound(id);
    }

    // --------------------------------------------------------------------------------------------
    // Метрики
    // --------------------------------------------------------------------------------------------

    ResourceManager::ResourceMetrics ResourceManager::getMetrics() const noexcept {
        ResourceMetrics metrics{};
        metrics.textureCount = mTextures.size();
        metrics.dynamicTextureCount = mDynamicTextures.size();
        metrics.fontCount = mFonts.size();
        metrics.dynamicFontCount = mDynamicFonts.size();
        metrics.soundCount = mSounds.size();
        metrics.dynamicSoundCount = mDynamicSounds.size();
        return metrics;
    }

    // --------------------------------------------------------------------------------------------
    // Preload API
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
        const auto& textureConfigs = paths::ResourcePaths::getAllTextureConfigs();
        for (const auto& [id, config] : textureConfigs) {
            (void) config;
            (void) getTexture(id);
        }
    }

    void ResourceManager::preloadAllFonts() {
        const auto& fontConfigs = paths::ResourcePaths::getAllFontConfigs();
        for (const auto& [id, config] : fontConfigs) {
            (void) config;
            (void) getFont(id);
        }
    }

    void ResourceManager::preloadAllSounds() {
        const auto& soundConfigs = paths::ResourcePaths::getAllSoundConfigs();
        for (const auto& [id, config] : soundConfigs) {
            (void) config;
            (void) getSound(id);
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
    }

} // namespace core::resources