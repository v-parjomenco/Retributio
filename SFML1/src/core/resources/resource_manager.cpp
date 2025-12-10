#include "pch.h"

#include "core/resources/resource_manager.h"
#include <cassert>
#include <stdexcept>

#include "core/log/log_macros.h"

namespace core::resources {

    namespace {

        using core::resources::config::FontResourceConfig;
        using core::resources::config::SoundResourceConfig;
        using core::resources::config::TextureResourceConfig;

        template <typename Cache, typename Key>
        const types::TextureResource&
        ensureTextureLoadedWithConfig(Cache&                       cache,
                                      const Key&                   key,
                                      const TextureResourceConfig& config) {
            bool loadedNow = false;

            if (!cache.contains(key)) {
                [[maybe_unused]] const bool wasLoaded = cache.load(key, config.path);

                assert(wasLoaded &&
                       "[ResourceManager] ResourceHolder::load(...) returned false "
                       "while !contains(key).");

                if (!wasLoaded) {
                    throw std::runtime_error("[ResourceManager::ensureTextureLoadedWithConfig] "
                                             "не удалось загрузить текстуру, path='" +
                                             config.path + "'");
                } else {
                    loadedNow = true;
                }
            }

            auto& textureResource = cache.get(key);

            // Если реально только что загрузили — применяем флаги
            if (loadedNow) {
                textureResource.setSmooth(config.smooth);
                textureResource.setRepeated(config.repeated);

                if (config.generateMipmap) {
                    if (!textureResource.generateMipmap()) {
                        LOG_WARN(core::log::cat::Resources,
                                 "[ResourceManager::ensureTextureLoadedWithConfig]\n"
                                 "Не удалось сгенерировать mipmap для текстуры: {}",
                                 config.path);
                    }
                }
            }

            return textureResource;
        }

        template <typename Cache, typename Key>
        const types::FontResource&
        ensureFontLoadedWithConfig(Cache& cache,
                                   const Key& key,
                                   const FontResourceConfig& config) {
            if (!cache.contains(key)) {
                [[maybe_unused]] const bool wasLoaded = cache.load(key, config.path);

                assert(wasLoaded &&
                       "[ResourceManager] ResourceHolder::load(...) returned false "
                       "while !contains(key).");

                if (!wasLoaded) {
                    throw std::runtime_error(
                        "[ResourceManager::ensureFontLoadedWithConfig] "
                        "не удалось загрузить шрифт, path='" + config.path + "'"
                    );
                }
            }

            return cache.get(key);
        }

        template <typename Cache, typename Key>
        const types::SoundBufferResource&
        ensureSoundLoadedWithConfig(Cache&                  cache,
                                    const Key&              key,
                                    const SoundResourceConfig& config) {
            if (!cache.contains(key)) {
                [[maybe_unused]] const bool wasLoaded = cache.load(key, config.path);

                assert(wasLoaded &&
                       "[ResourceManager] ResourceHolder::load(...) returned false "
                       "while !contains(key).");

                if (!wasLoaded) {
                    throw std::runtime_error(
                        "[ResourceManager::ensureSoundLoadedWithConfig] "
                        "не удалось загрузить звук, path='" + config.path + "'"
                    );
                }
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
                LOG_WARN(core::log::cat::Resources,
                         "[ResourceManager::getTexture(TextureID)]\n"
                         "Не удалось получить текстуру для ID: {}. "
                         "Используется fallback-текстура: {}. Детали: {}",
                         ids::toString(id),
                         ids::toString(mMissingTextureID),
                         exception.what());

                return getTexture(mMissingTextureID);
            }

            throw; // Fallback не задан — пусть ошибка поднимается выше.
        }
    }

    const types::TextureResource& ResourceManager::getTexture(const std::string& id) {
        try {
            TextureResourceConfig config{};
            config.path           = id;
            config.smooth         = true;
            config.repeated       = false;
            config.generateMipmap = false;

            return ensureTextureLoadedWithConfig(mDynamicTextures, id, config);
        } catch (const std::exception& exception) {
            if (mHasMissingTextureFallback) {
                LOG_WARN(core::log::cat::Resources,
                         "[ResourceManager::getTexture(std::string)]\n"
                         "Не удалось получить текстуру по строковому ID: {}. "
                         "Используется fallback-текстура: {}. Ошибка: {}",
                         id,
                         ids::toString(mMissingTextureID),
                         exception.what());

                return getTexture(mMissingTextureID);
            }

            throw;
        }
    }

    const types::TextureResource& ResourceManager::getTextureByPath(const std::string& path) {
        TextureResourceConfig config{};
        config.path           = path;
        config.smooth         = true;
        config.repeated       = false;
        config.generateMipmap = false;

        try {
            return ensureTextureLoadedWithConfig(mDynamicTextures, path, config);
        } catch (const std::exception& exception) {
            if (mHasMissingTextureFallback) {
                LOG_WARN(core::log::cat::Resources,
                         "[ResourceManager::getTextureByPath]\n"
                         "Не удалось загрузить текстуру по пути: {}. "
                         "Используется fallback-текстура: {}. Ошибка: {}",
                         path,
                         ids::toString(mMissingTextureID),
                         exception.what());

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
                LOG_WARN(core::log::cat::Resources,
                         "[ResourceManager::getFont(FontID)]\n"
                         "Не удалось получить шрифт для ID: {}. "
                         "Используется fallback-шрифт: {}. Ошибка: {}",
                         ids::toString(id),
                         ids::toString(mMissingFontID),
                         exception.what());

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
                LOG_WARN(core::log::cat::Resources,
                         "[ResourceManager::getFont(std::string)]\n"
                         "Не удалось получить шрифт по строковому ID: {}. "
                         "Используется fallback-шрифт: {}. Ошибка: {}",
                         id,
                         ids::toString(mMissingFontID),
                         exception.what());

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
                LOG_WARN(core::log::cat::Resources,
                         "[ResourceManager::getSound(SoundID)]\n"
                         "Не удалось получить звук для ID: {}. "
                         "Используется fallback-звук: {}. Ошибка: {}",
                         ids::toString(id),
                         ids::toString(mMissingSoundID),
                         exception.what());

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
                LOG_WARN(core::log::cat::Resources,
                         "[ResourceManager::getSound(std::string)]\n"
                         "Не удалось получить звук по строковому ID: {}. "
                         "Используется fallback-звук: {}. Ошибка: {}",
                         id,
                         ids::toString(mMissingSoundID),
                         exception.what());

                return getSound(mMissingSoundID);
            }

            throw;
        }
    }

    // --------------------------------------------------------------------------------------------
    // Fallback-ресурсы
    // --------------------------------------------------------------------------------------------

    void ResourceManager::setMissingTextureFallback(ids::TextureID id) {
        mMissingTextureID        = id;
        mHasMissingTextureFallback = true;

        // Прогреваем/валидируем fallback-текстуру сразу.
        (void)getTexture(id);
    }

    void ResourceManager::setMissingFontFallback(ids::FontID id) {
        mMissingFontID        = id;
        mHasMissingFontFallback = true;

        (void)getFont(id);
    }

    void ResourceManager::setMissingSoundFallback(ids::SoundID id) {
        mMissingSoundID        = id;
        mHasMissingSoundFallback = true;

        (void)getSound(id);
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
    // Preload API
    // --------------------------------------------------------------------------------------------

    void ResourceManager::preloadTexture(ids::TextureID id) {
        (void)getTexture(id);
    }

    void ResourceManager::preloadFont(ids::FontID id) {
        (void)getFont(id);
    }

    void ResourceManager::preloadSound(ids::SoundID id) {
        (void)getSound(id);
    }

    void ResourceManager::preloadAllTextures() {
        const auto& textureConfigs = paths::ResourcePaths::getAllTextureConfigs();
        for (const auto& [id, config] : textureConfigs) {
            (void)config;
            (void)getTexture(id);
        }
    }

    void ResourceManager::preloadAllFonts() {
        const auto& fontConfigs = paths::ResourcePaths::getAllFontConfigs();
        for (const auto& [id, config] : fontConfigs) {
            (void)config;
            (void)getFont(id);
        }
    }

    void ResourceManager::preloadAllSounds() {
        const auto& soundConfigs = paths::ResourcePaths::getAllSoundConfigs();
        for (const auto& [id, config] : soundConfigs) {
            (void)config;
            (void)getSound(id);
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