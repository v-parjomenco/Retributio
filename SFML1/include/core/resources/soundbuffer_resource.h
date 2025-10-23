#pragma once

#include <SFML/Audio.hpp>
#include "core/i_resource.h"

namespace core::resources {

    class SoundBufferResource : public core::IResource {
    public:
        SoundBufferResource() = default;
        ~SoundBufferResource() override = default;

        bool loadFromFile(const std::string& filename) override {
            return mBuffer.loadFromFile(filename);
        }

        sf::SoundBuffer& get() { return mBuffer; }
        const sf::SoundBuffer& get() const { return mBuffer; }

    private:
        sf::SoundBuffer mBuffer;
    };

} // namespace core::resources