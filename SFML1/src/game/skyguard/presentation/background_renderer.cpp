#include "pch.h"

#include "game/skyguard/presentation/background_renderer.h"

#include <cassert>
#include <cmath>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/View.hpp>

#include "core/resources/resource_manager.h"

namespace game::skyguard::presentation {

    void BackgroundRenderer::init(core::resources::ResourceManager& resources,
                                  const core::resources::TextureKey textureKey) {
        const sf::Texture& texture = resources.getTexture(textureKey).get();
        mTexture = &texture;
        mTextureSize = texture.getSize();

        // Контракт: повторяемость должна настраиваться в ресурсном реестре/лоадере.
        // Если фон не тайлится, первым делом проверь: repeated=true и loader setRepeated(true).
        assert(texture.isRepeated() &&
               "Background texture must be repeated (setRepeated(true) in loader).");

        // КРИТИЧНО: sf::Vertex value-init -> прозрачный цвет {0,0,0,0}.
        // Белый tint — дефолтное ожидаемое поведение.
        for (auto& v : mQuad) {
            v.color = sf::Color::White;
        }
    }

    void BackgroundRenderer::update(const sf::View& worldView) noexcept {
        if (!mTexture || mTextureSize.x == 0u || mTextureSize.y == 0u) {
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
            mStats = {};
#endif
            return;
        }

        const sf::Vector2f size = worldView.getSize();
        const sf::Vector2f center = worldView.getCenter();
        const sf::Vector2f topLeft = center - (size * 0.5f);

        const float texW = static_cast<float>(mTextureSize.x);
        const float texH = static_cast<float>(mTextureSize.y);

        float u0 = std::fmod(topLeft.x, texW);
        float v0 = std::fmod(topLeft.y, texH);
        if (u0 < 0.f) {
            u0 += texW;
        }
        if (v0 < 0.f) {
            v0 += texH;
        }

        const float u1 = u0 + size.x;
        const float v1 = v0 + size.y;

        mQuad[0].position = topLeft;
        mQuad[1].position = {topLeft.x + size.x, topLeft.y};
        mQuad[2].position = {topLeft.x + size.x, topLeft.y + size.y};
        mQuad[3].position = topLeft;
        mQuad[4].position = {topLeft.x + size.x, topLeft.y + size.y};
        mQuad[5].position = {topLeft.x, topLeft.y + size.y};

        mQuad[0].texCoords = {u0, v0};
        mQuad[1].texCoords = {u1, v0};
        mQuad[2].texCoords = {u1, v1};
        mQuad[3].texCoords = {u0, v0};
        mQuad[4].texCoords = {u1, v1};
        mQuad[5].texCoords = {u0, v1};

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        const std::uint32_t tilesX = static_cast<std::uint32_t>(std::ceil(size.x / texW));
        const std::uint32_t tilesY = static_cast<std::uint32_t>(std::ceil(size.y / texH));

        mStats.tilesCovered = tilesX * tilesY;
        mStats.drawCalls = 1;
        mStats.tileSize = mTextureSize;
        mStats.visibleRect = sf::FloatRect({topLeft.x, topLeft.y}, 
                                           {size.x, size.y});
        mStats.parallaxFactor = 1.0f;
#endif
    }

    void BackgroundRenderer::draw(sf::RenderWindow& window) const {
        if (!mTexture) {
            return;
        }

        sf::RenderStates states{};
        states.texture = mTexture;
        window.draw(mQuad, 6, sf::PrimitiveType::Triangles, states);
    }

} // namespace game::skyguard::presentation