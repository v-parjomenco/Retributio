// ================================================================================================
// File: core/config/properties/movement_properties.h
// Purpose: Data-only movement configuration brick (speed/accel/friction)
// Used by: PlayerBlueprint, future movable entity blueprints
// Related headers: core/config.h
// ================================================================================================
#pragma once

#include "core/config.h"

namespace core::config::properties {

/**
 * @brief Набор параметров, описывающих возможности движения сущности.
 *
 * Это не «игрок» и не «камера», а абстрактный набор чисел, который можно
 * использовать для любого движущегося объекта: корабля, пули, врага и т.п.
 */
struct MovementProperties {
    /// @brief Максимальная скорость (единиц в секунду).
    float speed = ::config::PLAYER_SPEED;

    /// @brief Ускорение (насколько быстро сущность разгоняется до скорости).
    float acceleration = ::config::PLAYER_ACCELERATION;

    /// @brief Коэффициент трения (демпфирования скорости).
    float friction = ::config::PLAYER_FRICTION;
};

} // namespace core::config::properties