#include "pch.h"

#include "core/resources/ids/resource_ids.h"

#include <cassert>

namespace core::resources::ids {

    const char* toString(TextureID id) noexcept {
        switch (id) {
        case TextureID::Player:
            return "Player";
        case TextureID::Unknown:
            return "UnknownTexture";
        }

        // Если сюда дошли — либо добавлен новый TextureID без обновления toString,
        // либо в программу попало некорректное значение (например, из повреждённого сейва).
        assert(false && "[TextureID] toString: unhandled value");
        return "InvalidTexture";
    }

    const char* toString(FontID id) noexcept {
        switch (id) {
        case FontID::Default:
            return "Default";
        case FontID::Unknown:
            return "UnknownFont";
        }

        assert(false && "[FontID] toString: unhandled value");
        return "InvalidFont";
    }

    const char* toString(SoundID id) noexcept {
        switch (id) {
        case SoundID::Unknown:
            return "UnknownSound";
        }

        assert(false && "[SoundID] toString: unhandled value");
        return "InvalidSound";
    }

} // namespace core::resources::ids