// ================================================================================================
// File: core/resources/loader/resource_loader.h
// Purpose: Mid-level resource loader for SFML resources (textures, fonts, sounds)
// Used by: ResourceManager / ResourceHolder
// Related headers: core/resources/types/texture_resource.h,
//                  core/resources/types/font_resource.h,
//                  core/resources/types/soundbuffer_resource.h
// ================================================================================================
#pragma once

#include <memory>
#include <string>

#include "core/resources/types/font_resource.h"
#include "core/resources/types/soundbuffer_resource.h"
#include "core/resources/types/texture_resource.h"

namespace core::resources::loader {


    // ResourceLoader - среднеуровневый модуль для загрузки ресурсов с диска.
    //
    // - Ниже, чем ResourceManager, но выше загрузки данных библиотекой SFML.
    // - Использует нативные методы SFML (loadFromFile) для Texture / Font / SoundBuffer.
    // - Возвращает std::unique_ptr<Resource>, который затем можно вставить в ResourceHolder
    // - Не управляет кэшированием и жизненным циклом
    // (этим занимаются ResourceManager/ResourceHolder).
    // - Позволяет изолировать код загрузки
    // (в будущем можно заменить на асинхронную/стриминговую загрузку).
    class ResourceLoader {
      public:
        // Загружает текстуру из файла.
        // Возвращает unique_ptr<TextureResource> или nullptr при ошибке.
        static std::unique_ptr<types::TextureResource> loadTexture(const std::string& path,
                                                                   bool smooth = true);
        // Загружает шрифт из файла.
        // Возвращает unique_ptr<FontResource> или nullptr при ошибке.
        static std::unique_ptr<types::FontResource> loadFont(const std::string& path);

        // Загружает звуковой буфер из файла.
        // Возвращает unique_ptr<SoundBufferResource> или nullptr при ошибке.
        static std::unique_ptr<types::SoundBufferResource> loadSoundBuffer(const std::string& path);
    };

} // namespace core::resources::loader