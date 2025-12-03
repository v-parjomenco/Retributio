// ================================================================================================
// File: core/resources/config/sound_resource_config.h
// Purpose: Data-only configuration for sound buffer resources (path, future low-level flags)
// Used by: ResourcePaths, ResourceManager, tools
// Notes:
//  - Describes how to locate raw sound data (SoundBuffer-level).
//  - Playback parameters (volume, looping, spatialization) live at a higher layer (Sound/AudioSystem).
// ================================================================================================
#pragma once

#include <string>

namespace core::resources::config {

    /**
     * @brief Конфигурация звукового ресурса (SoundBuffer).
     *
     * Сейчас описывает только путь к файлу. В будущем сюда можно добавить:
     *  - политику предзагрузки (preload) для критичных системных звуков;
     *  - флаги компрессии/формата;
     *  - режимы стриминга для длинных треков.
     */
    struct SoundResourceConfig {
        std::string path;

        // TODO: будущие параметры низкоуровневой загрузки звука:
        //  - prefetch/preload для системных звуков UI,
        //  - спец. режимы для длинных треков/музыки.
    };

} // namespace core::resources::config