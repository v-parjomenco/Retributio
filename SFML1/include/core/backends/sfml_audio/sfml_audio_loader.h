// ================================================================================================
// File: core/backends/sfml_audio/sfml_audio_loader.h
// Purpose: Factory for the SFML audio backend Loaders configuration.
//          Call makeSfmlAudioLoader() and pass the result to ResourceManager::initialize()
//          to wire up the SFML-backed sound factory.
// Used by: sfml1_skyguard and any other target that requires audio playback.
// ================================================================================================
#pragma once

#include "core/resources/resource_manager.h"

namespace core::resources::backends::sfml_audio {

    /// @brief Создаёт ResourceManager::Loaders с SFML-реализацией sound factory.
    ///
    /// Пример использования:
    ///   manager.initialize(sources, makeSfmlAudioLoader());
    [[nodiscard]] core::resources::ResourceManager::Loaders makeSfmlAudioLoader();

} // namespace core::resources::backends::sfml_audio