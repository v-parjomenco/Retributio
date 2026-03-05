// ================================================================================================
// File: core/resources/registry/resource_entry.h
// Purpose: Resource entry types for ResourceRegistry (runtime key + stable key + config).
// ================================================================================================
#pragma once

#include <cstdint>
#include <string_view>

#include "core/resources/config/font_resource_config.h"
#include "core/resources/config/sound_resource_config.h"
#include "core/resources/config/texture_resource_config.h"
#include "core/resources/keys/resource_key.h"

namespace core::resources {

    template <typename Key, typename Config>
    struct ResourceEntry {
        Key key{};                      // RuntimeKey32
        std::uint64_t stableKey = 0u;   // StableKey64 (xxHash3_64)
        std::string_view name{};        // interned canonical key
        std::string_view path{};        // interned path
        Config config{};                // type-specific config
    };

    using TextureEntry = ResourceEntry<TextureKey, config::TextureResourceConfig>;
    using FontEntry = ResourceEntry<FontKey, config::FontResourceConfig>;
    using SoundEntry = ResourceEntry<SoundKey, config::SoundResourceConfig>;

} // namespace core::resources