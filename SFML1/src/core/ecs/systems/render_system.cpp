#include "pch.h"

#include "core/ecs/systems/render_system.h"

#include <algorithm>
#include <chrono>
#include <cstddef>

#include "core/ecs/components/sprite_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/world.h"
#include "core/log/log_macros.h"

namespace {

    [[nodiscard]] sf::IntRect makeFullRect(const sf::Texture& texture) noexcept {
        const auto ts = texture.getSize();
        return sf::IntRect(sf::Vector2i{0, 0},
                           sf::Vector2i{static_cast<int>(ts.x), static_cast<int>(ts.y)});
    }

    [[nodiscard]] bool floatNotEqual(const float a, const float b) noexcept {
        // Строго соответствует логике сортировки: "не равны" == (a<b || b<a)
        // (заодно не полагаемся на != для float).
        return (a < b) || (b < a);
    }

    void appendSpriteTriangles(std::vector<sf::Vertex>& out,
                               const sf::Vector2f& position,
                               const sf::Vector2f& origin,
                               const sf::Vector2f& scale,
                               const sf::IntRect& rect) {
        const float w = static_cast<float>(rect.size.x);
        const float h = static_cast<float>(rect.size.y);

        const float ox = origin.x;
        const float oy = origin.y;

        const float sx = scale.x;
        const float sy = scale.y;

        const float px = position.x;
        const float py = position.y;

        // Локальные точки как у sf::Sprite (без rotation):
        // (-origin) .. (w-origin, h-origin), затем scale, затем translate.
        const sf::Vector2f p0{((-ox)     * sx) + px, ((-oy)     * sy) + py};
        const sf::Vector2f p1{((w - ox)  * sx) + px, ((-oy)     * sy) + py};
        const sf::Vector2f p2{((w - ox)  * sx) + px, ((h - oy)  * sy) + py};
        const sf::Vector2f p3{((-ox)     * sx) + px, ((h - oy)  * sy) + py};

        const float u0 = static_cast<float>(rect.position.x);
        const float v0 = static_cast<float>(rect.position.y);
        const float u1 = u0 + w;
        const float v1 = v0 + h;

        const sf::Vector2f t0{u0, v0};
        const sf::Vector2f t1{u1, v0};
        const sf::Vector2f t2{u1, v1};
        const sf::Vector2f t3{u0, v1};

        const sf::Color color = sf::Color::White;

        // 6 вершин одним расширением буфера — меньше накладных расходов, чем 6 push_back.
        const std::size_t base = out.size();
        out.resize(base + 6);

        out[base + 0].position = p0;
        out[base + 0].color = color;
        out[base + 0].texCoords = t0;

        out[base + 1].position = p1;
        out[base + 1].color = color;
        out[base + 1].texCoords = t1;

        out[base + 2].position = p2;
        out[base + 2].color = color;
        out[base + 2].texCoords = t2;

        out[base + 3].position = p0;
        out[base + 3].color = color;
        out[base + 3].texCoords = t0;

        out[base + 4].position = p2;
        out[base + 4].color = color;
        out[base + 4].texCoords = t2;

        out[base + 5].position = p3;
        out[base + 5].color = color;
        out[base + 5].texCoords = t3;
    }

} // namespace

namespace core::ecs {

    RenderSystem::FrameStats RenderSystem::getLastFrameStats() const noexcept {
    #ifndef NDEBUG
        return mLastStats;
    #else
        return FrameStats{};
    #endif
    }

    void RenderSystem::render(World& world, sf::RenderWindow& window) {

#ifndef NDEBUG
        const auto totalStart = std::chrono::steady_clock::now();
        std::uint64_t drawUs = 0;
        std::size_t textureSwitches = 0;
#endif

        // ----------------------------------------------------------------------------------------
        // Шаг 1: Gather — собираем лёгкие ключи (без копирования Transform/rect/scale/origin).
        // ----------------------------------------------------------------------------------------

        mKeys.clear();

        auto view = world.view<TransformComponent, SpriteComponent>();
        mKeys.reserve(view.size_hint());

        for (auto [entity, transform, sprite] : view.each()) {
            (void)transform; // намеренно не используем на фазе Gather

            mKeys.push_back(RenderKey{
                .zOrder = sprite.zOrder,
                .textureId = sprite.textureId,
                .entity = entity
            });
        }

        if (mKeys.empty()) {
#ifndef NDEBUG
            mLastStats = FrameStats{};
#endif
            return;
        }

        // ----------------------------------------------------------------------------------------
        // Шаг 2: Sort — детерминированно и batching-friendly.
        // ----------------------------------------------------------------------------------------

        std::sort(mKeys.begin(), mKeys.end(),
                  [](const RenderKey& a, const RenderKey& b) {
                      if (a.zOrder < b.zOrder) {
                          return true;
                      }
                      if (b.zOrder < a.zOrder) {
                          return false;
                      }

                      if (a.textureId != b.textureId) {
                          return a.textureId < b.textureId;
                      }

                      return core::ecs::toUint(a.entity) < core::ecs::toUint(b.entity);
                  });

        // ----------------------------------------------------------------------------------------
        // Шаг 3: Draw — Vertex batching (CPU). Flush на смене zOrder или textureId.
        // ----------------------------------------------------------------------------------------

        mVertices.clear();
        mVertices.reserve(mKeys.size() * 6);

        sf::RenderStates states;

        core::resources::ids::TextureID currentTextureId = mKeys.front().textureId;
        float currentZ = mKeys.front().zOrder;

        const sf::Texture* currentTexture = &mResources->getTexture(currentTextureId).get();
        states.texture = currentTexture;

        sf::IntRect fullRect = makeFullRect(*currentTexture);

        std::size_t batchDrawCalls = 0;

        const auto flush = [&]() {
            if (mVertices.empty()) {
                return;
            }

#ifndef NDEBUG
            const auto drawStart = std::chrono::steady_clock::now();
#endif

            window.draw(mVertices.data(),
                        mVertices.size(),
                        sf::PrimitiveType::Triangles,
                        states);

#ifndef NDEBUG
            const auto drawEnd = std::chrono::steady_clock::now();
            drawUs += static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(drawEnd - drawStart).count());
#endif

            mVertices.clear();
            ++batchDrawCalls;
        };

        for (const auto& key : mKeys) {
            const Entity entity = key.entity;

            const auto& transform = view.get<TransformComponent>(entity);
            const auto& spr = view.get<SpriteComponent>(entity);

            const bool zChanged = floatNotEqual(key.zOrder, currentZ);
            const bool textureChanged = (key.textureId != currentTextureId);

            if (zChanged || textureChanged) {
                flush();

                currentZ = key.zOrder;

                if (textureChanged) {
                    currentTextureId = key.textureId;
                    currentTexture = &mResources->getTexture(currentTextureId).get();
                    states.texture = currentTexture;

                    fullRect = makeFullRect(*currentTexture);

#ifndef NDEBUG
                    ++textureSwitches;
#endif
                }
            }

            const sf::IntRect rect =
                (spr.textureRect.size.x > 0 && spr.textureRect.size.y > 0)
                    ? spr.textureRect
                    : fullRect;

            appendSpriteTriangles(mVertices,
                                  transform.position,
                                  spr.origin,
                                  spr.scale,
                                  rect);
        }

        flush();

        // ----------------------------------------------------------------------------------------
        // Диагностика
        // ----------------------------------------------------------------------------------------

#ifndef NDEBUG
        const auto totalEnd = std::chrono::steady_clock::now();
        const std::uint64_t totalUs = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(totalEnd - totalStart).count());
        FrameStats stats{};
        stats.spriteCount = mKeys.size();
        stats.vertexCount = stats.spriteCount * 6;
        stats.batchDrawCalls = batchDrawCalls;
        stats.textureSwitches = textureSwitches;
        stats.cpuTotalUs = totalUs;
        stats.cpuDrawUs = drawUs;
        mLastStats = stats;

        // Редкий лог (DEV-only): не спамим, только раз в ~60 кадров и только при изменении count.
        static int frameCount = 0;
        static std::size_t lastLoggedCount = static_cast<std::size_t>(-1);

        ++frameCount;

        if (frameCount == 1 || frameCount % 60 == 0) {
            const std::size_t entityCount = mKeys.size();
            if (entityCount != lastLoggedCount) {
                LOG_DEBUG(core::log::cat::ECS,
                          "RenderSystem: {} sprites, {} draw-calls (batched), frame {}",
                          entityCount, batchDrawCalls, frameCount);
                lastLoggedCount = entityCount;
            }
        }
#endif
    }

} // namespace core::ecs