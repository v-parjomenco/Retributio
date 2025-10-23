#include "core/resources/resource_manager.h"

namespace core {

    // Текстуры

    // Статический ID, из enum class в resourceIDs
    const resources::TextureResource& ResourceManager::getTexture(resources::TextureID id, bool smooth) {
        if (!mTextures.contains(id)) {
            const std::string path = resources::ResourcePaths::get(id); // путь берём из JSON
            mTextures.load(id, path);
            mTextures.get(id).setSmooth(smooth);
        }
        return mTextures.get(id);
    }
    // Динамический ID, из JSON
    const resources::TextureResource& ResourceManager::getTexture(const std::string& id, bool smooth) {
        if (!mDynamicTextures.contains(id)) {
            const std::string path = id; // id здесь уже строка пути
            mDynamicTextures.load(id, path);
            mDynamicTextures.get(id).setSmooth(smooth);
        }
        return mDynamicTextures.get(id);
    }

    // Шрифты

    // Статический ID, из enum class в resourceIDs
    const resources::FontResource& ResourceManager::getFont(resources::FontID id) {
        if (!mFonts.contains(id)) {
            const std::string path = resources::ResourcePaths::get(id); // путь берём из JSON
            mFonts.load(id, path);
        }
        return mFonts.get(id);
    }
    // Динамический ID, из JSON
    const resources::FontResource& ResourceManager::getFont(const std::string& id) {
        if (!mDynamicFonts.contains(id)) {
            mDynamicFonts.load(id, id); // id = путь к файлу
        }
        return mDynamicFonts.get(id);
    }

    // Звуки

    // Статический ID, из enum class в resourceIDs
    const resources::SoundBufferResource& ResourceManager::getSound(resources::SoundID id) {
        if (!mSounds.contains(id)) {
            const std::string path = resources::ResourcePaths::get(id);
            mSounds.load(id, path);
        }
        return mSounds.get(id);
    }
    // Динамический ID, из JSON
    const resources::SoundBufferResource& ResourceManager::getSound(const std::string& id) {
        if (!mDynamicSounds.contains(id)) {
            mDynamicSounds.load(id, id); // id = путь к файлу
        }
        return mDynamicSounds.get(id);
    }

} // namespace core