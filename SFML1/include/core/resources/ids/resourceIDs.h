// ================================================================================================
// File: core/resources/ids/resourceIDs.h
// Purpose: Enum identifiers for engine resources (textures, fonts, sounds)
// Used by: ResourceManager, ResourcePaths, id_to_string, gameplay code
// Related headers: id_to_string.h, resource_paths.h
// ================================================================================================
#pragma once

#include <cstdint>

namespace core::resources::ids {

    // --------------------------------------------------------------------------------------------
    // Текстуры
    // --------------------------------------------------------------------------------------------
    enum class TextureID : std::uint8_t {
        Unknown = 0,
        Player,
        // TODO: add more texture IDs here (e.g. UIFrame, BackgroundMain, etc.)
    };

    // --------------------------------------------------------------------------------------------
    // Шрифты
    // --------------------------------------------------------------------------------------------
    enum class FontID : std::uint8_t {
        Unknown = 0,
        Default,
        // TODO: add more font IDs here (e.g. DebugMono, TitleFont, etc.)
    };

    // --------------------------------------------------------------------------------------------
    // Звуки
    // --------------------------------------------------------------------------------------------
    enum class SoundID : std::uint8_t {
        Unknown = 0,
        // TODO: add sound IDs when you start using sounds in resources.json
    };

    // --------------------------------------------------------------------------------------------
    // toString(...) — человекочитаемые имена для логов и ошибок
    // --------------------------------------------------------------------------------------------

    inline const char* toString(TextureID id) noexcept {
        switch (id) {
        case TextureID::Player:
            return "Player";
        case TextureID::Unknown:
        default:
            return "UnknownTexture";
        }
    }

    inline const char* toString(FontID id) noexcept {
        switch (id) {
        case FontID::Default:
            return "Default";
        case FontID::Unknown:
        default:
            return "UnknownFont";
        }
    }

    inline const char* toString(SoundID id) noexcept {
        switch (id) {
        case SoundID::Unknown:
        default:
            return "UnknownSound";
        }
    }

} // namespace core::resources::ids
