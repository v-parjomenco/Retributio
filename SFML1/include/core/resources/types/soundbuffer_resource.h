#pragma once

#include <SFML/Audio.hpp>

#include "core/resources/i_resource.h"

namespace core::resources::types {

    class SoundBufferResource : public core::resources::IResource {
      public:
        SoundBufferResource() = default;
        ~SoundBufferResource() override = default;

        bool loadFromFile(const std::string& filename) override {
            return mBuffer.loadFromFile(filename);
        }

        sf::SoundBuffer& get() {
            return mBuffer;
        }
        const sf::SoundBuffer& get() const {
            return mBuffer;
        }

      private:
        sf::SoundBuffer mBuffer;
    };

} // namespace core::resources::types