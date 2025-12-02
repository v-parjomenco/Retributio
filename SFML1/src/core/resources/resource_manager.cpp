#include "pch.h"

#include "core/resources/resource_manager.h"
#include <cassert>
#include <stdexcept>

#include "core/resources/loader/resource_loader.h"
#include "core/utils/message.h"

namespace core::resources {

    namespace {
        namespace message = core::utils::message;
    }

    // --------------------------------------------------------------------------------------------
    // Текстуры
    // --------------------------------------------------------------------------------------------

    const types::TextureResource& ResourceManager::getTexture(ids::TextureID id, bool smooth) {
        try {
            if (!mTextures.contains(id)) {
                const std::string& path = paths::ResourcePaths::get(id);

                [[maybe_unused]] const bool wasLoaded = mTextures.load(id, path);
                assert(wasLoaded && "[ResourceManager::getTexture(TextureID)] "
                                    "load() вернул false при !contains(id).");

                // Флаг сглаживания применяется только при первой загрузке ресурса.
                mTextures.get(id).setSmooth(smooth);
            }

            return mTextures.get(id);
        } catch (const std::exception& exception) {
            // Если задан fallback и это не сам fallback-ID — пробуем вернуть его.
            if (mHasMissingTextureFallback && id != mMissingTextureID) {
                message::logDebug(std::string("[ResourceManager::getTexture(TextureID)]\n"
                                              "Не удалось получить текстуру для ID: ") +
                                  std::string(ids::toString(id)) +
                                  ". Используем fallback-текстуру: " +
                                  std::string(ids::toString(mMissingTextureID)) +
                                  ". Ошибка: " + exception.what());

                // Если fallback также сломан — исключение пробросится наружу.
                return getTexture(mMissingTextureID, smooth);
            }

            throw; // Fallback не задан — пусть ошибка поднимается выше.
        }
    }

    const types::TextureResource& ResourceManager::getTexture(const std::string& id, bool smooth) {
        try {
            if (!mDynamicTextures.contains(id)) {
                [[maybe_unused]] const bool wasLoaded = mDynamicTextures.load(id, id);
                assert(wasLoaded && "[ResourceManager::getTexture(std::string)] "
                                    "load() вернул false при !contains(id).");

                mDynamicTextures.get(id).setSmooth(smooth);
            }

            return mDynamicTextures.get(id);
        } catch (const std::exception& exception) {
            if (mHasMissingTextureFallback) {
                message::logDebug(std::string("[ResourceManager::getTexture(std::string)]\n"
                                              "Не удалось получить текстуру по пути/ID: ") +
                                  id + ". Используем fallback-текстуру: " +
                                  std::string(ids::toString(mMissingTextureID)) +
                                  ". Ошибка: " + exception.what());

                return getTexture(mMissingTextureID, smooth);
            }

            throw;
        }
    }

    const types::TextureResource& ResourceManager::getTextureByPath(const std::string& path,
                                                                    bool smooth) {
        // Если ресурс с таким путём уже загружен — просто отдаём его.
        if (mDynamicTextures.contains(path)) {
            return mDynamicTextures.get(path);
        }

        auto texturePointer = loader::ResourceLoader::loadTexture(path, smooth);
        if (!texturePointer) {
            if (mHasMissingTextureFallback) {
                message::logDebug(std::string("[ResourceManager::getTextureByPath]\n"
                                              "Не удалось загрузить текстуру по пути: ") +
                                  path + ". Используем fallback-текстуру: " +
                                  std::string(ids::toString(mMissingTextureID)));

                return getTexture(mMissingTextureID, smooth);
            }

            throw std::runtime_error(std::string("[ResourceManager::getTextureByPath]\n"
                                                 "Не удалось загрузить текстуру: ") +
                                     path);
        }

        // Вставляем уже загруженный ресурс в ResourceHolder, чтобы избежать повторной загрузки.
        mDynamicTextures.insert(path, std::move(texturePointer));
        return mDynamicTextures.get(path);
    }

    // --------------------------------------------------------------------------------------------
    // Шрифты
    // --------------------------------------------------------------------------------------------

    const types::FontResource& ResourceManager::getFont(ids::FontID id) {
        try {
            if (!mFonts.contains(id)) {
                const std::string& path = paths::ResourcePaths::get(id);

                [[maybe_unused]] const bool wasLoaded = mFonts.load(id, path);
                assert(wasLoaded && "[ResourceManager::getFont(FontID)] "
                                    "load() вернул false при !contains(id).");
            }

            return mFonts.get(id);
        } catch (const std::exception& exception) {
            if (mHasMissingFontFallback && id != mMissingFontID) {
                message::logDebug(std::string("[ResourceManager::getFont(FontID)]\n"
                                              "Не удалось получить шрифт для ID: ") +
                                  std::string(ids::toString(id)) + ". Используем fallback-шрифт: " +
                                  std::string(ids::toString(mMissingFontID)) +
                                  ". Ошибка: " + exception.what());

                return getFont(mMissingFontID);
            }

            throw;
        }
    }

    const types::FontResource& ResourceManager::getFont(const std::string& id) {
        try {
            if (!mDynamicFonts.contains(id)) {
                [[maybe_unused]] const bool wasLoaded = mDynamicFonts.load(id, id);
                assert(wasLoaded && "[ResourceManager::getFont(std::string)] "
                                    "load() вернул false при !contains(id).");
            }

            return mDynamicFonts.get(id);
        } catch (const std::exception& exception) {
            if (mHasMissingFontFallback) {
                message::logDebug(std::string("[ResourceManager::getFont(std::string)]\n"
                                              "Не удалось получить шрифт по пути/ID: ") +
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
            if (!mSounds.contains(id)) {
                const std::string& path = paths::ResourcePaths::get(id);

                [[maybe_unused]] const bool wasLoaded = mSounds.load(id, path);
                assert(wasLoaded && "[ResourceManager::getSound(SoundID)] "
                                    "load() вернул false при !contains(id).");
            }

            return mSounds.get(id);
        } catch (const std::exception& exception) {
            if (mHasMissingSoundFallback && id != mMissingSoundID) {
                message::logDebug(std::string("[ResourceManager::getSound(SoundID)]\n"
                                              "Не удалось получить звук для ID: ") +
                                  std::string(ids::toString(id)) + ". Используем fallback-звук: " +
                                  std::string(ids::toString(mMissingSoundID)) +
                                  ". Ошибка: " + exception.what());

                return getSound(mMissingSoundID);
            }

            throw;
        }
    }

    const types::SoundBufferResource& ResourceManager::getSound(const std::string& id) {
        try {
            if (!mDynamicSounds.contains(id)) {
                [[maybe_unused]] const bool wasLoaded = mDynamicSounds.load(id, id);
                assert(wasLoaded && "[ResourceManager::getSound(std::string)] "
                                    "load() вернул false при !contains(id).");
            }

            return mDynamicSounds.get(id);
        } catch (const std::exception& exception) {
            if (mHasMissingSoundFallback) {
                message::logDebug(std::string("[ResourceManager::getSound(std::string)]\n"
                                              "Не удалось получить звук по пути/ID: ") +
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
    // Preload API (массовая предварительная загрузка)
    // --------------------------------------------------------------------------------------------

    void ResourceManager::preloadAllTextures() {
        // Идём по реестру путей и прогреваем каждую текстуру.
        for (const auto& entry : paths::ResourcePaths::getAllTexturePaths()) {
            const auto textureID = entry.first;
            (void) getTexture(textureID);
        }
    }

    void ResourceManager::preloadAllFonts() {
        for (const auto& entry : paths::ResourcePaths::getAllFontPaths()) {
            const auto fontID = entry.first;
            (void) getFont(fontID);
        }
    }

    void ResourceManager::preloadAllSounds() {
        for (const auto& entry : paths::ResourcePaths::getAllSoundPaths()) {
            const auto soundID = entry.first;
            (void) getSound(soundID);
        }
    }

    // --------------------------------------------------------------------------------------------
    // Метрики
    // --------------------------------------------------------------------------------------------

    ResourceManager::ResourceMetrics ResourceManager::getMetrics() const noexcept {
        ResourceMetrics metrics{};

        // На этом этапе используем только размер хранилищ.
        // Позже сюда можно добавить суммарную память и прочие счётчики.
        metrics.textureCount = mTextures.size();
        metrics.dynamicTextureCount = mDynamicTextures.size();

        metrics.fontCount = mFonts.size();
        metrics.dynamicFontCount = mDynamicFonts.size();

        metrics.soundCount = mSounds.size();
        metrics.dynamicSoundCount = mDynamicSounds.size();

        return metrics;
    }

} // namespace core::resources