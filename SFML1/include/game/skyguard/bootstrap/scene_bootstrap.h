// ================================================================================================
// File: game/skyguard/bootstrap/scene_bootstrap.h
// Purpose: Scene bootstrap for SkyGuard: resolve keys, preload resources, compute derived data.
// Used by: game/skyguard/game.cpp (initWorld wiring).
// Notes:
//  - This is game-layer bootstrap logic (NOT core).
//  - Performs validate-on-write at the scene boundary.
// ================================================================================================
#pragma once

#include <optional>
#include <span>

#include "core/resources/keys/resource_key.h"
#include "game/skyguard/config/blueprints/player_blueprint.h"

namespace core::resources {
    class ResourceManager;
}

namespace game::skyguard::bootstrap {

    struct SceneBootstrapConfig {
        // in/out: bootstrap заполняет resolvedTextureRect/resolvedOrigin.
        std::span<game::skyguard::config::blueprints::PlayerBlueprint> players;
    };

    struct SceneBootstrapResult {
        core::resources::TextureKey backgroundKey{};
        core::resources::FontKey defaultFontKey{};
        std::optional<core::resources::TextureKey> stressPlayerKey{};
    };

    [[nodiscard]] SceneBootstrapResult preloadAndResolveInitialScene(
        core::resources::ResourceManager& resources,
        SceneBootstrapConfig& config);

} // namespace game::skyguard::bootstrap
