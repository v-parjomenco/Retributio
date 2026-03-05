// ================================================================================================
// File: core/backends/sfml_audio/sfml_sound_resource.h
// Purpose: SFML-backed implementation of core::resources::ISoundResource.
// Notes:
//  - Private to retributio_sfml_audio target. Do not include from core or game code.
//  - This header lives in include_private/ and is reachable ONLY from retributio_sfml_audio
//    via target_include_directories(PRIVATE). External targets must not be able to
//    resolve this header by include paths.
// ================================================================================================
#pragma once

#include <SFML/Audio/SoundBuffer.hpp>

#include "core/resources/types/i_sound_resource.h"

namespace core::resources::backends::sfml_audio {

    class SfmlSoundResource final : public core::resources::ISoundResource {
      public:
        SfmlSoundResource() = default;
        ~SfmlSoundResource() override = default;

        [[nodiscard]] bool loadFromFile(std::string_view path) override;
        [[nodiscard]] const sf::SoundBuffer& buffer() const noexcept;

      private:
        sf::SoundBuffer mBuffer;
    };

} // namespace core::resources::backends::sfml_audio