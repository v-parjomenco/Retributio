#include "pch.h"

#include "game/skyguard/presentation/background_renderer.h"

#include <cassert>
#include <cstdint>

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/View.hpp>

#include "core/log/log_macros.h"
#include "core/resources/resource_manager.h"
#include "game/skyguard/presentation/background_math.h"

namespace game::skyguard::presentation {

    void BackgroundRenderer::init(core::resources::ResourceManager& resources,
                                  const core::resources::TextureKey textureKey) {
        mResources = &resources;
        mTextureKey = textureKey;
        mCachedResourceGen = resources.cacheGeneration();

        // Resident-only: init не должен вызывать loading API.
        const sf::Texture& texture = resources.expectTextureResident(textureKey).get();
        mTexture = &texture;
        mTextureSize = texture.getSize();

        // Контракт: фон ОБЯЗАН быть повторяемым. Это authoring/programmer error, ловим и в Release.
        if (!texture.isRepeated()) {
            LOG_PANIC(core::log::cat::Resources,
                      "[BackgroundRenderer::init] Background texture must be repeated "
                      "(setRepeated(true) in loader/registry). texKeyIdx={}",
                      textureKey.valid() ? textureKey.index() : 0u);
        }

        // КРИТИЧНО: sf::Vertex value-init -> прозрачный цвет {0,0,0,0}.
        // Белый tint — дефолтное ожидаемое поведение.
        for (auto& v : mQuad) {
            v.color = sf::Color::White;
        }
    }

    void BackgroundRenderer::update(const sf::View& worldView) noexcept {
        // Обновляем кэш текстуры при инвалидировании ресурсных кэшей (clearAll / hot-reload).
        if (mResources != nullptr) {
            const std::uint32_t gen = mResources->cacheGeneration();
            if (gen != mCachedResourceGen) {
                const sf::Texture& texture = mResources->expectTextureResident(mTextureKey).get();
                mTexture = &texture;
                mTextureSize = texture.getSize();
                mCachedResourceGen = gen;

                // Контракт repeated должен оставаться истинным (иначе фон перестанет тайлиться).
                if (!texture.isRepeated()) {
                    LOG_PANIC(core::log::cat::Resources,
                              "[BackgroundRenderer::update] Background texture must be repeated "
                              "(setRepeated(true) in loader/registry). texKeyIdx={}",
                              mTextureKey.valid() ? mTextureKey.index() : 0u);
                }
            }
        }

        if (mTexture == nullptr || mTextureSize.x == 0u || mTextureSize.y == 0u) {
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
            mStats = {};
#endif
            return;
        }

        const sf::Vector2f size = worldView.getSize();
        const sf::Vector2f center = worldView.getCenter();
        const sf::Vector2f topLeft = center - (size * 0.5f);

        // texH нужен всегда для v0/v1.
        const float texH = static_cast<float>(mTextureSize.y);

        const float u0 = 0.0f;      // X зафиксирован контрактом камеры
        const float offsetY = 0.0f; // параллакса сейчас нет, но API готов

        const float startY = computeTileStartY(topLeft.y, offsetY, texH);
        const float v0 = topLeft.y - startY;

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
        const float texW = static_cast<float>(mTextureSize.x);

        const std::uint32_t tilesX =
            static_cast<std::uint32_t>(computeTileCount(size.x, texW));
        const std::uint32_t tilesY =
            static_cast<std::uint32_t>(computeTileCount(size.y, texH));

        mStats.tilesCovered = tilesX * tilesY;
        mStats.drawCalls = 1;
        mStats.tileSize = mTextureSize;
        mStats.visibleRect = sf::FloatRect({topLeft.x, topLeft.y}, {size.x, size.y});
        mStats.parallaxFactor = 1.0f;
#endif
    }

    void BackgroundRenderer::draw(sf::RenderWindow& window) const {
        if (mTexture == nullptr) {
            return;
        }

        sf::RenderStates states{}; // value-init: детерминированные дефолты
        states.texture = mTexture;
        window.draw(mQuad, 6, sf::PrimitiveType::Triangles, states);
    }

} // namespace game::skyguard::presentation
