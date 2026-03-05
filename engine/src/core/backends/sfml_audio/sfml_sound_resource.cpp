#include "pch.h"

#include "core/backends/sfml_audio/sfml_sound_resource.h"

#include <filesystem>

namespace core::resources::backends::sfml_audio {

    bool SfmlSoundResource::loadFromFile(std::string_view path) {
        const std::filesystem::path nativePath{path.begin(), path.end()};
        return mBuffer.loadFromFile(nativePath);
    }

    const sf::SoundBuffer& SfmlSoundResource::buffer() const noexcept {
        return mBuffer;
    }

} // namespace core::resources::backends::sfml_audio