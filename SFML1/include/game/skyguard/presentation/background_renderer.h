// ================================================================================================
// File: game/skyguard/presentation/background_renderer.h
// Purpose: Tiled background renderer for SkyGuard (world space).
// Used by: Game render pass (world view).
// ================================================================================================
#pragma once

#include <cstdint>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/Vertex.hpp>
#include <SFML/System/Vector2.hpp>

#include "core/resources/keys/resource_key.h"

namespace sf {
    class RenderWindow;
    class Texture;
    class View;
} // namespace sf

namespace core::resources {
    class ResourceManager;
}

namespace game::skyguard::presentation {

    /**
     * @brief Рендерит повторяемую текстуру фона, закрывающую видимую область мира.
     *
     * Контракт:
     *  - НЕ ECS-система.
     *  - Использует повторяемую текстуру из ResourceManager.
     *  - Рисуется в world-pass до игровых сущностей.
     */
    class BackgroundRenderer {
      public:
        struct BackgroundStats {
            std::uint32_t tilesCovered{0};
            std::uint32_t drawCalls{0};
            sf::Vector2u tileSize{0u, 0u};
            sf::FloatRect visibleRect{};
            float parallaxFactor{1.0f};
        };
        void init(core::resources::ResourceManager& resources,
                  core::resources::TextureKey texture);

        void update(const sf::View& worldView) noexcept;
        void draw(sf::RenderWindow& window) const;

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        [[nodiscard]] const BackgroundStats& getLastFrameStats() const noexcept {
            return mStats;
        }
#endif

      private:
        const sf::Texture* mTexture{nullptr};
        sf::Vertex mQuad[6]{};
        sf::Vector2u mTextureSize{0u, 0u};

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        BackgroundStats mStats{};
#endif
    };

} // namespace game::skyguard::presentation