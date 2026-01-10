// ================================================================================================
// File: game/skyguard/ecs/components/player_tag_component.h
// Purpose: Player identity markers (player index + local player tag).
// Used by: PlayerInitSystem, Game camera update.
// ================================================================================================
#pragma once

#include <cstdint>

namespace game::skyguard::ecs {

    /**
     * @brief Marker component — identifies player entity.
     *
     * Multiplayer (max 2 players):
     *  - Player 1: has PlayerTagComponent + LocalPlayerTagComponent
     *  - Player 2: has PlayerTagComponent only (or own LocalPlayerTag on their client)
     */
    struct PlayerTagComponent {
        std::uint8_t playerIndex{0}; // 0 = player 1, 1 = player 2
    };

    /**
     * @brief Marker — identifies LOCAL player for camera follow.
     *
     * In single-player: exactly 1 entity has this.
     * NOT serialized for replay/network (presentation-only).
     */
    struct LocalPlayerTagComponent {};

} // namespace game::skyguard::ecs