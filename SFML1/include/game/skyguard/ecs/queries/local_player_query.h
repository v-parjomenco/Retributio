// ================================================================================================
// File: game/skyguard/ecs/queries/local_player_query.h
// Purpose: O(1) local player entity query (view-based, no linear scan)
// Used by: Game::updateCamera, Game::processEvents, overlay_extras
// ================================================================================================
#pragma once

#include "core/ecs/components/transform_component.h"
#include "core/ecs/entity.h"
#include "core/ecs/world.h"
#include "game/skyguard/ecs/components/player_tag_component.h"

namespace game::skyguard::ecs::queries {

    // O(1) поиск локального игрока: первая сущность из view<LocalPlayerTag, Transform>.
    // Возвращает false, если игрок не найден.
    //
    // Вызывается каждый кадр (updateCamera, overlay). Стоимость:
    // view.begin() по двум storage — O(1) amortised (EnTT sparse set iteration).
    [[nodiscard]] inline bool tryGetLocalPlayerTransform(
        core::ecs::World& world,
        core::ecs::Entity& outEntity,
        const core::ecs::TransformComponent*& outTransform) noexcept {

        auto view = world.view<game::skyguard::ecs::LocalPlayerTagComponent,
                               core::ecs::TransformComponent>();

        const auto it = view.begin();
        if (it == view.end()) {
            outEntity = core::ecs::NullEntity;
            outTransform = nullptr;
            return false;
        }

        const core::ecs::Entity e = *it;
        outEntity = e;
        outTransform = &view.get<core::ecs::TransformComponent>(e);
        return true;
    }

} // namespace game::skyguard::ecs::queries