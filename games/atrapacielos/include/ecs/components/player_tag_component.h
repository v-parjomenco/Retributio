// ================================================================================================
// File: ecs/components/player_tag_component.h
// Purpose: Player identity markers (hot path, PlayerTag=1 byte, LocalTag=1 byte).
// Used by: PlayerInitSystem, Game camera update.
// ================================================================================================
#pragma once

#include <cstdint>
#include <type_traits>

namespace game::atrapacielos::ecs {

    /**
     * @brief Компонент-маркер — идентифицирует сущность игрока.
     *
     * Мультиплеер (макс. 2 игрока):
     *  - Игрок 1: имеет PlayerTagComponent + LocalPlayerTagComponent;
     *  - Игрок 2: имеет только PlayerTagComponent (или свой LocalPlayerTag у себя на клиенте).
     */
    struct PlayerTagComponent {
        std::uint8_t playerIndex{0}; // 0 = игрок 1, 1 = игрок 2
    };

    /**
     * @brief Маркер, идентифицирующий ЛОКАЛЬНОГО игрока для следования камеры.
     *
     * В одиночной игре: ровно 1 сущность имеет этот тег.
     * НЕ сериализуется для replay/network (presentation-only).
     */
    struct LocalPlayerTagComponent {};

    static_assert(sizeof(PlayerTagComponent) == 1,
                  "PlayerTagComponent: размер должен оставаться 1 байт.");
    static_assert(alignof(PlayerTagComponent) == 1,
                  "PlayerTagComponent: выравнивание должно оставаться 1 байт.");
    static_assert(std::is_trivially_copyable_v<PlayerTagComponent>,
                  "PlayerTagComponent должен быть trivially copyable.");
    static_assert(std::is_standard_layout_v<PlayerTagComponent>,
                  "PlayerTagComponent должен иметь standard layout.");

    static_assert(sizeof(LocalPlayerTagComponent) == 1,
                  "LocalPlayerTagComponent: размер должен оставаться 1 байт.");
    static_assert(alignof(LocalPlayerTagComponent) == 1,
                  "LocalPlayerTagComponent: выравнивание должно оставаться 1 байт.");
    static_assert(std::is_trivially_copyable_v<LocalPlayerTagComponent>,
                  "LocalPlayerTagComponent должен быть trivially copyable.");
    static_assert(std::is_standard_layout_v<LocalPlayerTagComponent>,
                  "LocalPlayerTagComponent должен иметь standard layout.");

} // namespace game::atrapacielos::ecs