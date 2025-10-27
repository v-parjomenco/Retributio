#pragma once

#include <memory>
#include <string>

#include "core/resources/texture_resource.h"
#include "core/resources/font_resource.h"
#include "core/resources/soundbuffer_resource.h"

namespace core::resources {

    /*
     ResourceLoader - низкоуровневый модуль для загрузки ресурсов с диска.
     - Возвращает std::unique_ptr<Resource>, который затем можно вставить в ResourceHolder
     - Не управляет кэшированием/идентификаторами — этим занимается ResourceManager / ResourceHolder.
     - Позволяет изолировать код загрузки (и в будущем - сделать его асинхронным).
    */  
    class ResourceLoader {
    public:
        // Возвращает unique_ptr на загруженный TextureResource или nullptr, если загрузка не удалась.
        static std::unique_ptr<TextureResource> loadTexture(const std::string& path, bool smooth = true);

        // Возвращает уникальный указатель на загруженный FontResource или nullptr при ошибке.
        static std::unique_ptr<FontResource> loadFont(const std::string& path);

        // Возвращает уникальный указатель на загруженный SoundBufferResource или nullptr при ошибке.
        static std::unique_ptr<SoundBufferResource> loadSoundBuffer(const std::string& path);
    };

} // namespace core::resources