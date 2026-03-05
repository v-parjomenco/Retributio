// ================================================================================================
// File: config/blueprints/player_blueprint.h
// Purpose: High-level player blueprint assembled from generic properties
// Used by: ConfigLoader, PlayerInitSystem
// Related headers: core/config/properties/*.h
// ================================================================================================
#pragma once

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include "core/config/properties/movement_properties.h"
#include "core/config/properties/sprite_properties.h"
#include "config/properties/aircraft_control_bindings_properties.h"
#include "config/properties/aircraft_control_properties.h"

namespace game::atrapacielos::config::blueprints {

    /**
     * @brief Высокоуровневый конфиг сущности игрока.
     *
     * Это не «сырые данные из JSON», а уже собранный blueprint из маленьких
     * кирпичиков properties::*. Его задача — описать:
     *
     *  - какой спрайт использовать и с каким масштабом;
     *  - как быстро и с каким трением игрок двигается;
     *  - стартовую позицию в мире (в world space);
     *  - какими клавишами управляется.
     *
     * Важно: этот blueprint относится к конкретной игре (Atrapacielos),
     * но использует максимально переиспользуемые properties из ядра.
     */
    struct PlayerBlueprint {
        core::config::properties::SpriteProperties sprite;
        core::config::properties::MovementProperties movement;
        game::atrapacielos::config::properties::AircraftControlBindingsProperties controls;
        game::atrapacielos::config::properties::AircraftControlProperties aircraftControl;
        sf::Vector2f startPosition{0.f, 0.f};

        // ----------------------------------------------------------------------------------------
        // Resolved fields (bootstrap-only, validate-on-write)
        // ----------------------------------------------------------------------------------------
        // Эти поля НЕ читаются из JSON. Они заполняются на границе сцены (scene bootstrap)
        // после preload: derived данные вычисляются ОДИН раз и дальше используются в ECS без
        // зависимостей от ResourceManager/SFML Texture в update-системах.
        //
        // Контракт:
        //  - resolvedTextureRect.size должен быть >0 (иначе это "не resolved" -> programmer error).
        //  - resolvedOrigin должен соответствовать выбранной политике (в Atrapacielos: центр текстуры).
        sf::IntRect resolvedTextureRect{};
        sf::Vector2f resolvedOrigin{0.f, 0.f};
    };

} // namespace game::atrapacielos::config::blueprints
