// ================================================================================================
// File: core/resources/handles/sound_handle.h
// Purpose: Lightweight value handle for resident sound resources.
// Used by: ResourceManager API consumers and backend playback adapters.
// ================================================================================================
#pragma once

#include <cstdint>
#include <limits>

namespace core::resources {

    struct SoundHandle {
        static constexpr std::uint32_t Invalid = std::numeric_limits<std::uint32_t>::max();
        std::uint32_t index = Invalid;

        [[nodiscard]] bool valid() const noexcept {
            return index != Invalid;
        }

        // Только equality — ordering для runtime cache index не имеет доменной семантики.
        bool operator==(const SoundHandle&) const = default;
    };

} // namespace core::resources
