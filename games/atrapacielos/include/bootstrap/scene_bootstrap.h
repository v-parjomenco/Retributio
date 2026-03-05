// ================================================================================================
// File: bootstrap/scene_bootstrap.h
// Purpose: Scene bootstrap for Atrapacielos: resolve keys, preload resources, compute derived data.
// Used by: game.cpp (initWorld wiring).
// Notes:
//  - This is game-layer bootstrap logic (NOT core).
//  - Performs validate-on-write at the scene boundary.
// ================================================================================================
#pragma once

#include <optional>
#include <span>

#include "core/resources/keys/resource_key.h"
#include "config/blueprints/player_blueprint.h"

namespace core::resources {
    class ResourceManager;
}

namespace game::atrapacielos::bootstrap {

    struct SceneBootstrapConfig {
        // in/out: bootstrap заполняет resolvedTextureRect/resolvedOrigin.
        std::span<game::atrapacielos::config::blueprints::PlayerBlueprint> players;
    };

    struct SceneBootstrapResult {
        core::resources::TextureKey backgroundKey{};
        core::resources::FontKey defaultFontKey{};
        std::optional<core::resources::TextureKey> stressPlayerKey{};
    };

    [[nodiscard]] SceneBootstrapResult preloadAndResolveInitialScene(
        core::resources::ResourceManager& resources,
        SceneBootstrapConfig& config);

} // namespace game::atrapacielos::bootstrap
