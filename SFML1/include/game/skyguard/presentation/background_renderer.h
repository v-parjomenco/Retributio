// ================================================================================================
// File: game/skyguard/presentation/background_renderer.h
// Purpose: Tiled background renderer for SkyGuard (world space).
// Used by: Game render pass (world view).
// ================================================================================================
#pragma once

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Vertex.hpp>
#include <SFML/System/Vector2.hpp>

#include "core/resources/resource_manager.h"
#include "core/resources/ids/resource_ids.h"

namespace game::skyguard::presentation {

    /**
     * @brief Renders a repeated background texture covering the visible world view.
     *
     * Contract:
     *  - NOT an ECS system.
     *  - Uses a repeated texture from ResourceManager.
     *  - Draws in world pass before gameplay entities.
     */
    class BackgroundRenderer {
      public:
        void init(core::resources::ResourceManager& resources,
                  core::resources::ids::TextureID textureId);

        void update(const sf::View& worldView) noexcept;
        void draw(sf::RenderWindow& window) const;

      private:
        const sf::Texture* mTexture{nullptr};
        sf::Vertex mQuad[6]{};
        sf::Vector2u mTextureSize{0u, 0u};
    };

} // namespace game::skyguard::presentation