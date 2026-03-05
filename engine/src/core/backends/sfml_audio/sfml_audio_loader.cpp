#include "pch.h"

#include "core/backends/sfml_audio/sfml_audio_loader.h"
#include <memory>

#include "core/backends/sfml_audio/sfml_sound_resource.h"

namespace core::resources::backends::sfml_audio {
    namespace {
        [[nodiscard]] std::unique_ptr<core::resources::ISoundResource> createSfmlSound() {
            return std::make_unique<SfmlSoundResource>();
        }
    } // namespace
    core::resources::ResourceManager::Loaders makeSfmlAudioLoader() {
        core::resources::ResourceManager::Loaders loaders{};
        loaders.bindSoundFactory<createSfmlSound>();
        return loaders;
    }
} // namespace core::resources::backends::sfml_audio