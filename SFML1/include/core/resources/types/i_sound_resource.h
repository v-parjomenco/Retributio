// ================================================================================================
// File: core/resources/types/i_sound_resource.h
// Purpose: Backend-agnostic sound resource contract for ResourceManager.
// Used by: core::resources::ResourceManager and audio backend modules.
// Notes:
//  - Contains no SFML-specific types.
//  - Implementations own backend objects and control their lifetime via RAII.
// ================================================================================================
#pragma once

#include <string_view>

namespace core::resources {

    // Playback API намеренно отсутствует на текущем этапе. ISoundResource отвечает
    // только за владение загруженными звуковыми данными, не за воспроизведение.
    // 
    // TODO: AudioPlaybackSystem / AudioService в sfml1_sfml_audio модуле.
    // Система принимает SoundHandle, делает lookup через
    // ResourceManager::tryGetSoundResource(), downcast к backend-типу внутри
    // backend-модуля (не в game-коде). Core остаётся backend-agnostic.
    class ISoundResource {
      public:
        virtual ~ISoundResource() = default;

        ISoundResource(const ISoundResource&) = delete;
        ISoundResource& operator=(const ISoundResource&) = delete;
        ISoundResource(ISoundResource&&) = delete;
        ISoundResource& operator=(ISoundResource&&) = delete;

        [[nodiscard]] virtual bool loadFromFile(std::string_view path) = 0;

      protected:
        ISoundResource() = default;
    };

} // namespace core::resources