#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>

#include "core/resources/types/texture_resource.h"
#include "core/resources/types/font_resource.h"
#include "core/resources/types/soundbuffer_resource.h"
#include "core/resources/holders/resource_holder.h"
#include "core/resources/ids/resourceIDs.h"
#include "core/resources/paths/resource_paths.h"


namespace core::resources {

    class ResourceManager {
    public:

        ResourceManager() = default;
        ~ResourceManager() = default;

        // Текстуры

        // Статический ID, из enum class в resourceIDs
        const types::TextureResource& getTexture(ids::TextureID id, bool smooth = true);
        // Динамический ID, из JSON
        const types::TextureResource& getTexture(const std::string& id, bool smooth = true);
        // Получить текстуру по пути (explicit path)
        const types::TextureResource& getTextureByPath(const std::string& path, bool smooth = true);

        // Шрифты

        // Статический ID, из enum class в resourceIDs
        const types::FontResource& getFont(ids::FontID id);
        // Динамический ID, из JSON
        const types::FontResource& getFont(const std::string& id);

        // Звуки

        // Статический ID, из enum class в resourceIDs
        const types::SoundBufferResource& getSound(ids::SoundID id);
        // Динамический ID, из JSON
        const types::SoundBufferResource& getSound(const std::string& id);

    private:
        // ResourceHolder для статических ID
        holders::ResourceHolder<types::TextureResource, ids::TextureID> mTextures;
        holders::ResourceHolder<types::FontResource, ids::FontID> mFonts;
        holders::ResourceHolder<types::SoundBufferResource, ids::SoundID> mSounds;

        // ResourceHolder для динамических ключей
        holders::ResourceHolder<types::TextureResource, std::string> mDynamicTextures;
        holders::ResourceHolder<types::FontResource, std::string> mDynamicFonts;
        holders::ResourceHolder<types::SoundBufferResource, std::string> mDynamicSounds;
    };

} // namespace core::resources
