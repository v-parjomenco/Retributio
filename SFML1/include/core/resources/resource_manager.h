#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>

#include "core/resources/texture_resource.h"
#include "core/resources/font_resource.h"
#include "core/resources/resource_holder.h"
#include "core/resources/resourceIDs.h"
#include "core/resources/resource_paths.h"
#include "core/resources/soundbuffer_resource.h"

namespace core::resources {

    class ResourceManager {
    public:

        ResourceManager() = default;
        ~ResourceManager() = default;

        // Текстуры

        // Статический ID, из enum class в resourceIDs
        const resources::TextureResource& getTexture(resources::TextureID id, bool smooth = true);
        // Динамический ID, из JSON
        const resources::TextureResource& getTexture(const std::string& id, bool smooth = true);
        // Получить текстуру по пути (explicit path)
        const resources::TextureResource& getTextureByPath(const std::string& path, bool smooth = true);

        // Шрифты

        // Статический ID, из enum class в resourceIDs
        const resources::FontResource& getFont(resources::FontID id);
        // Динамический ID, из JSON
        const resources::FontResource& getFont(const std::string& id);

        // Звуки

        // Статический ID, из enum class в resourceIDs
        const resources::SoundBufferResource& getSound(resources::SoundID id);
        // Динамический ID, из JSON
        const resources::SoundBufferResource& getSound(const std::string& id);

    private:
        // ResourceHolder для статических ID
        ResourceHolder<resources::TextureResource, resources::TextureID> mTextures;
        ResourceHolder<resources::FontResource, resources::FontID> mFonts;
        ResourceHolder<resources::SoundBufferResource, resources::SoundID> mSounds;

        // ResourceHolder для динамических ключей
        ResourceHolder<resources::TextureResource, std::string> mDynamicTextures;
        ResourceHolder<resources::FontResource, std::string> mDynamicFonts;
        ResourceHolder<resources::SoundBufferResource, std::string> mDynamicSounds;
    };

} // namespace core::resources
