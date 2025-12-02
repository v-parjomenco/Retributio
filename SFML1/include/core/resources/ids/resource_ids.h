// ================================================================================================
// File: core/resources/ids/resource_ids.h
// Purpose: Enum identifiers for engine resources (textures, fonts, sounds)
// Used by: ResourceManager, ResourcePaths, id_to_string, gameplay code
// Notes:
//  - Enum values describe the *current build's* resource set (e.g. SkyGuard).
//  - For multi-game setups these IDs can be split per-game while keeping the same patterns.
// ================================================================================================
#pragma once

#include <cstdint>
#include <functional>
#include <type_traits>

namespace core::resources::ids {

    // --------------------------------------------------------------------------------------------
    // Текстуры
    // --------------------------------------------------------------------------------------------
    enum class TextureID : std::uint16_t {
        Unknown = 0,
        Player,
        // TODO: add more texture IDs here (e.g. UIFrame, BackgroundMain, etc.)
    };

    // --------------------------------------------------------------------------------------------
    // Шрифты
    // --------------------------------------------------------------------------------------------
    enum class FontID : std::uint16_t {
        Unknown = 0,
        Default,
        // TODO: add more font IDs here (e.g. DebugMono, TitleFont, etc.)
    };

    // --------------------------------------------------------------------------------------------
    // Звуки
    // --------------------------------------------------------------------------------------------
    enum class SoundID : std::uint16_t {
        Unknown = 0,
        // TODO: add sound IDs when you start using sounds in resources.json
    };

    // --------------------------------------------------------------------------------------------
    // toString(...) — человекочитаемые имена для логов и ошибок
    // --------------------------------------------------------------------------------------------

    const char* toString(TextureID id) noexcept;
    const char* toString(FontID id) noexcept;
    const char* toString(SoundID id) noexcept;

} // namespace core::resources::ids

    // --------------------------------------------------------------------------------------------
// Hash-специализации для resource ids
// ------------------------------------------------------------------------------------------------
namespace std {

    template <> struct hash<core::resources::ids::TextureID> {
        std::size_t operator()(core::resources::ids::TextureID id) const noexcept {
            using Underlying = std::underlying_type_t<core::resources::ids::TextureID>;
            return std::hash<Underlying>{}(static_cast<Underlying>(id));
        }
    };

    template <> struct hash<core::resources::ids::FontID> {
        std::size_t operator()(core::resources::ids::FontID id) const noexcept {
            using Underlying = std::underlying_type_t<core::resources::ids::FontID>;
            return std::hash<Underlying>{}(static_cast<Underlying>(id));
        }
    };

    template <> struct hash<core::resources::ids::SoundID> {
        std::size_t operator()(core::resources::ids::SoundID id) const noexcept {
            using Underlying = std::underlying_type_t<core::resources::ids::SoundID>;
            return std::hash<Underlying>{}(static_cast<Underlying>(id));
        }
    };

} // namespace std
