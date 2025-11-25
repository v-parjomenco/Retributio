// ================================================================================================
// File: core/config/properties/movement_properties.h
// Purpose: Data-only movement configuration brick (speed/accel/friction)
// Used by: PlayerBlueprint, future movable entity blueprints
// Related headers: (none, pure data brick)
// ================================================================================================
#pragma once

namespace core::config::properties {

    /**
     * @brief Набор параметров, описывающих возможности движения сущности.
     *
     * Это не «игрок» и не «камера», а абстрактный набор чисел, который можно
     * использовать для любого движущегося объекта: корабля, пули, врага и т.п.
     *
     * ВАЖНО:
     *  - здесь нет knowledge о "правильных" скоростях;
     *  - источником истинных значений является конкретная игра (blueprint/JSON).
     */
    struct MovementProperties {
        /// @brief Максимальная скорость (единиц в секунду).
        float speed = 0.f;

        /// @brief Ускорение (насколько быстро сущность разгоняется до скорости).
        float acceleration = 0.f;

        /// @brief Коэффициент трения (демпфирования скорости).
        float friction = 0.f;
    };

} // namespace core::config::properties