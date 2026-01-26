// ================================================================================================
// File: game/skyguard/config/blueprints/player_blueprint.h
// Purpose: High-level player blueprint assembled from generic properties
// Used by: ConfigLoader, PlayerInitSystem
// Related headers: core/config/properties/*.h
// ================================================================================================
#pragma once

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include "core/config/properties/movement_properties.h"
#include "core/config/properties/sprite_properties.h"
#include "game/skyguard/config/properties/aircraft_control_bindings_properties.h"
#include "game/skyguard/config/properties/aircraft_control_properties.h"

namespace game::skyguard::config::blueprints {

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
     * Важно: этот blueprint относится к конкретной игре (Skyguard),
     * но использует максимально переиспользуемые properties из ядра.
     */
    struct PlayerBlueprint {
        core::config::properties::SpriteProperties sprite;
        core::config::properties::MovementProperties movement;
        game::skyguard::config::properties::AircraftControlBindingsProperties controls;
        game::skyguard::config::properties::AircraftControlProperties aircraftControl;
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
        //  - resolvedOrigin должен соответствовать выбранной политике (в SkyGuard: центр текстуры).
        sf::IntRect resolvedTextureRect{};
        sf::Vector2f resolvedOrigin{0.f, 0.f};
    };

} // namespace game::skyguard::config::blueprints
