#include "pch.h"

#include "core/ecs/systems/render_system.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>

#include <SFML/System/Angle.hpp>

#include "core/ecs/components/spatial_handle_component.h"
#include "core/ecs/components/sprite_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/world.h"
#include "core/log/log_macros.h"
#include "core/resources/resource_manager.h"  // mResources->getTexture()
#include "core/spatial/aabb2.h"
#include "core/spatial/spatial_index.h"       // mSpatialIndex->query()
#include "core/utils/math_constants.h"

namespace {

#if !defined(NDEBUG)
    [[nodiscard]] bool isFiniteVec2(const sf::Vector2f& v) noexcept {
        return std::isfinite(v.x) && std::isfinite(v.y);
    }
#endif

    [[nodiscard]] core::spatial::Aabb2 makeViewAabb(const sf::View& view) noexcept {
        // Culling работает только для неповернутого view (getRotation() == 0).
        // При повороте AABB не описывает видимую область корректно.
        const sf::Vector2f size = view.getSize();
        const sf::Vector2f center = view.getCenter();
        const sf::Vector2f topLeft{center.x - size.x * 0.5f, center.y - size.y * 0.5f};

        core::spatial::Aabb2 out{};
        out.minX = topLeft.x;
        out.minY = topLeft.y;
        out.maxX = topLeft.x + size.x;
        out.maxY = topLeft.y + size.y;
        return out;
    }

    [[nodiscard]] bool floatNotEqual(const float a, const float b) noexcept {
        // ВАЖНО: должно строго соответствовать strict weak ordering в sort (без epsilon!).
        // "Не равны" означает, что один строго меньше другого (a < b || b < a).
        return (a < b) || (b < a);
    }

    void writeSpriteTriangles(sf::Vertex* out,
                              const sf::Vector2f& position,
                              const sf::Vector2f& origin,
                              const sf::Vector2f& scale,
                              const sf::IntRect& rect) noexcept {
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
        const sf::Vector2f p0{((-ox) * sx) + px, ((-oy) * sy) + py};
        const sf::Vector2f p1{((w - ox) * sx) + px, ((-oy) * sy) + py};
        const sf::Vector2f p2{((w - ox) * sx) + px, ((h - oy) * sy) + py};
        const sf::Vector2f p3{((-ox) * sx) + px, ((h - oy) * sy) + py};

        const float u0 = static_cast<float>(rect.position.x);
        const float v0 = static_cast<float>(rect.position.y);
        const float u1 = u0 + w;
        const float v1 = v0 + h;

        const sf::Vector2f t0{u0, v0};
        const sf::Vector2f t1{u1, v0};
        const sf::Vector2f t2{u1, v1};
        const sf::Vector2f t3{u0, v1};

        const sf::Color color = sf::Color::White;

        out[0].position = p0;
        out[0].color = color;
        out[0].texCoords = t0;
        out[1].position = p1;
        out[1].color = color;
        out[1].texCoords = t1;
        out[2].position = p2;
        out[2].color = color;
        out[2].texCoords = t2;
        out[3].position = p0;
        out[3].color = color;
        out[3].texCoords = t0;
        out[4].position = p2;
        out[4].color = color;
        out[4].texCoords = t2;
        out[5].position = p3;
        out[5].color = color;
        out[5].texCoords = t3;
    }

    void writeSpriteTrianglesRotated(sf::Vertex* out,
                                     const sf::Vector2f& position,
                                     const sf::Vector2f& origin,
                                     const sf::Vector2f& scale,
                                     const sf::IntRect& rect,
                                     const float cachedSin,
                                     const float cachedCos) noexcept {
        const float w = static_cast<float>(rect.size.x);
        const float h = static_cast<float>(rect.size.y);

        const float ox = origin.x;
        const float oy = origin.y;

        const float sx = scale.x;
        const float sy = scale.y;

        const sf::Vector2f l0{(-ox) * sx, (-oy) * sy};
        const sf::Vector2f l1{(w - ox) * sx, (-oy) * sy};
        const sf::Vector2f l2{(w - ox) * sx, (h - oy) * sy};
        const sf::Vector2f l3{(-ox) * sx, (h - oy) * sy};

        const auto rotate = [&](const sf::Vector2f& v) noexcept -> sf::Vector2f {
            return {(v.x * cachedCos) - (v.y * cachedSin),
                    (v.x * cachedSin) + (v.y * cachedCos)};
        };

        const sf::Vector2f p0 = rotate(l0) + position;
        const sf::Vector2f p1 = rotate(l1) + position;
        const sf::Vector2f p2 = rotate(l2) + position;
        const sf::Vector2f p3 = rotate(l3) + position;

        const float u0 = static_cast<float>(rect.position.x);
        const float v0 = static_cast<float>(rect.position.y);
        const float u1 = u0 + w;
        const float v1 = v0 + h;

        const sf::Vector2f t0{u0, v0};
        const sf::Vector2f t1{u1, v0};
        const sf::Vector2f t2{u1, v1};
        const sf::Vector2f t3{u0, v1};

        const sf::Color color = sf::Color::White;

        out[0].position = p0;
        out[0].color = color;
        out[0].texCoords = t0;
        out[1].position = p1;
        out[1].color = color;
        out[1].texCoords = t1;
        out[2].position = p2;
        out[2].color = color;
        out[2].texCoords = t2;
        out[3].position = p0;
        out[3].color = color;
        out[3].texCoords = t0;
        out[4].position = p2;
        out[4].color = color;
        out[4].texCoords = t2;
        out[5].position = p3;
        out[5].color = color;
        out[5].texCoords = t3;
    }

} // namespace

namespace core::ecs {

    RenderSystem::FrameStats RenderSystem::getLastFrameStats() const noexcept {
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        return mLastStats;
#else
        return FrameStats{};
#endif
    }

    // --------------------------------------------------------------------------------------------
    // КРИТИЧНО: render() намеренно НЕ noexcept.
    // sf::RenderWindow::draw() может выбросить исключение (GPU errors, driver issues).
    // Исключения пробрасываются в Game layer для корректной обработки.
    // --------------------------------------------------------------------------------------------

    void RenderSystem::render(World& world, sf::RenderWindow& window) {
#if !defined(NDEBUG)
        assert(window.getView().getRotation() == sf::Angle::Zero &&
               "RenderSystem: view rotation is not supported (view.getRotation() must be 0).");
        assert(mSpatialIndex != nullptr && "RenderSystem: bind() не вызван — mSpatialIndex null");
        assert(mResources != nullptr && "RenderSystem: bind() не вызван — mResources null");
#endif

        // View-culling в world-space координатах текущего view.
        const core::spatial::Aabb2 viewAabb = makeViewAabb(window.getView());

#if defined(SFML1_PROFILE)
        const auto totalStart = std::chrono::steady_clock::now();
        std::uint64_t drawUs = 0;
#endif

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        std::size_t totalCandidates = 0;
        std::size_t culled = 0;
        std::size_t textureSwitches = 0;
        std::size_t resourceLookupsThisFrame = 0;
        mUniqueTexturePointers.clear();
#endif

#if defined(SFML1_PROFILE)
        std::uint64_t gatherUs = 0;
        std::uint64_t sortUs = 0;
        std::uint64_t buildUs = 0;
        const auto gatherStart = std::chrono::steady_clock::now();
#endif

        // ------------------------------------------------------------------------------------
        // Шаг 1: Gather + Culling — собираем ключи только для видимых спрайтов.
        // ------------------------------------------------------------------------------------

        // Amortized growth: reserve(count + max(count/2, 256)).
        // Избегаем повторных реаллокаций при постепенном увеличении visible count.
        const auto ensureCapacity = [](auto& vec, std::size_t count) {
            const std::size_t current = vec.capacity();
            if (count > current) {
                const std::size_t grow = std::max(count / 2, std::size_t{256});
                vec.reserve(count + grow);
            }
        };

        ensureCapacity(mVisibleEntities, mLastVisibleCount);

        mVisibleEntities.clear();
        mKeys.clear();
        mPackets.clear();
        mTextureCache.clear();

        mSpatialIndex->query(viewAabb, mVisibleEntities);

        const std::size_t visibleCount = mVisibleEntities.size();
        mLastVisibleCount = visibleCount;

        ensureCapacity(mKeys, visibleCount);
        ensureCapacity(mPackets, visibleCount);

        // ------------------------------------------------------------------------------------
        // КРИТИЧНАЯ ОПТИМИЗАЦИЯ: Vertex buffer без инициализации.
        // std::vector<sf::Vertex>::resize(N) зануляет ~7MB при 50k sprites.
        // std::make_unique_for_overwrite<sf::Vertex[]> (C++20) = zero init cost.
        // ------------------------------------------------------------------------------------
        const std::size_t maxVertices = visibleCount * 6;
        if (maxVertices > mVertexBufferCapacity) {
            const std::size_t grow = std::max(maxVertices / 2, std::size_t{1536});
            const std::size_t newCapacity = maxVertices + grow;
            mVertexBuffer = std::make_unique_for_overwrite<sf::Vertex[]>(newCapacity);
            mVertexBufferCapacity = newCapacity;
        }

        // Per-frame cache: TextureID → sf::Texture*.
        // Fast-path: после сортировки по textureId большинство sprites имеют одинаковую текстуру.
        // Храним TextureID + индекс вместо указателя (указатели инвалидируются при реаллокации).
        core::resources::ids::TextureID lastTextureId = core::resources::ids::TextureID::Unknown;
        std::size_t lastTextureIndex = 0;

        const auto getTextureEntryCached =
            [&](core::resources::ids::TextureID id) -> const TextureCacheEntry& {
            // Fast-path: проверка последней текстуры за O(1).
            // ВАЖНО: проверка !mTextureCache.empty() нужна для защиты от UB, 
            // если id совпадает с начальным lastTextureId до первого добавления.
            if (!mTextureCache.empty() && 
                core::resources::ids::toUnderlying(id) ==
                core::resources::ids::toUnderlying(lastTextureId)) {
                return mTextureCache[lastTextureIndex];
            }

            // Поиск в существующем кэше.
            for (std::size_t i = 0; i < mTextureCache.size(); ++i) {
                if (core::resources::ids::toUnderlying(mTextureCache[i].id) ==
                    core::resources::ids::toUnderlying(id)) {
                    lastTextureId = id;
                    lastTextureIndex = i;
                    return mTextureCache[i];
                }
            }

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
            ++resourceLookupsThisFrame;
#endif

            const sf::Texture& tex = mResources->getTexture(id).get();
            TextureCacheEntry entry{};
            entry.id = id;
            entry.texture = &tex;
            mTextureCache.push_back(entry);

            lastTextureId = id;
            lastTextureIndex = mTextureCache.size() - 1;

            return mTextureCache.back();
        };

        auto ecsView = world.view<TransformComponent, SpriteComponent, SpatialHandleComponent>();

        // Итерация по mVisibleEntities (уже отфильтровано SpatialIndex).
        // Если число видимых сущностей будет постоянно превышать 50k, поменять на: 
        //  packed iteration через view.each() + inline culling.

        for (const Entity entity : mVisibleEntities) {
#if !defined(NDEBUG)
            assert(ecsView.contains(entity) &&
                   "RenderSystem: SpatialIndex вернул entity без (Transform, Sprite, SpatialHandle)");
#endif
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
            ++totalCandidates;
#endif
            const auto& spr = ecsView.get<SpriteComponent>(entity);
            const auto& tr = ecsView.get<TransformComponent>(entity);
            const auto& sh = ecsView.get<SpatialHandleComponent>(entity);

#if !defined(NDEBUG)
            const bool dataFinite = isFiniteVec2(tr.position) &&
                                    std::isfinite(tr.rotationDegrees) && isFiniteVec2(spr.origin) &&
                                    isFiniteVec2(spr.scale) && std::isfinite(spr.zOrder);
            // Это engine-level инвариант: NaN/Inf сюда попадать не должны.
            assert(dataFinite && "RenderSystem: NaN/Inf в Transform/Sprite данных");
            if (!dataFinite) {
                ++culled;
                continue;
            }
#endif

            const sf::IntRect rect = spr.textureRect;
#if !defined(NDEBUG)
            assert((rect.size.x > 0 && rect.size.y > 0) &&
                   "RenderSystem: textureRect должен иметь положительный размер");
#endif

            // SpatialIndex возвращает cell-level кандидатов.
            // Fine AABB culling обязателен для pixel-accurate visibility.
            // Entity в ячейке может НЕ пересекаться с viewAabb, даже если ячейка пересекается.
            if (!core::spatial::intersectsInclusive(sh.lastAabb, viewAabb)) {
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
                ++culled;
#endif
                continue;
            }

            // Sin/cos считаем только для реально отрисуемых (после culling).
            const float rotationDegrees = tr.rotationDegrees;
            const bool isRotated = (rotationDegrees != 0.f);
            float cachedSin = 0.f;
            float cachedCos = 1.f;
            if (isRotated) {
                const float radians = rotationDegrees * core::utils::kDegToRad;
                cachedSin = std::sin(radians);
                cachedCos = std::cos(radians);
            }

            const std::size_t packetIdx = mPackets.size();
            mPackets.push_back(RenderPacket{.position = tr.position,
                                            .origin = spr.origin,
                                            .scale = spr.scale,
                                            .rect = rect,
                                            .cachedSin = cachedSin,
                                            .cachedCos = cachedCos,
                                            .isRotated = isRotated});

            // packetIdx должен умещаться в uint32_t (max ~4 млрд).
            assert(packetIdx <= std::numeric_limits<std::uint32_t>::max() &&
                   "RenderSystem: packetIndex overflow");

            mKeys.push_back(RenderKey{.zOrder = spr.zOrder,
                                      .textureId = spr.textureId,
                                      .tieBreak = core::ecs::toUint(entity),
                                      .packetIndex = static_cast<std::uint32_t>(packetIdx)});
        }

#if defined(SFML1_PROFILE)
        {
            const auto gatherEnd = std::chrono::steady_clock::now();
            gatherUs = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(gatherEnd - gatherStart)
                    .count());
        }
#endif

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        ensureCapacity(mUniqueTexturePointers, mTextureCache.size());
#endif

        if (mKeys.empty()) {
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
            FrameStats stats{};
            stats.totalSpriteCount = totalCandidates;
            stats.culledSpriteCount = culled;
            stats.spriteCount = 0;
            stats.vertexCount = 0;
            stats.batchDrawCalls = 0;
            stats.textureSwitches = 0;
            stats.uniqueTexturePointers = 0;
            stats.textureCacheSize = mTextureCache.size();
            stats.resourceLookupsThisFrame = resourceLookupsThisFrame;
            mLastStats = stats;
#endif
            return;
        }

        // ------------------------------------------------------------------------------------
        // Шаг 2: Sort — детерминированно и batching-friendly.
        // ------------------------------------------------------------------------------------

#if defined(SFML1_PROFILE)
        const auto sortStart = std::chrono::steady_clock::now();
#endif

        std::sort(mKeys.begin(), mKeys.end(), [](const RenderKey& a, const RenderKey& b) {
            if (a.zOrder < b.zOrder) {
                return true;
            }
            if (b.zOrder < a.zOrder) {
                return false;
            }

            const auto aTex = core::resources::ids::toUnderlying(a.textureId);
            const auto bTex = core::resources::ids::toUnderlying(b.textureId);
            if (aTex < bTex) {
                return true;
            }
            if (bTex < aTex) {
                return false;
            }

            return a.tieBreak < b.tieBreak;
        });

#if defined(SFML1_PROFILE)
        {
            const auto sortEnd = std::chrono::steady_clock::now();
            sortUs = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(sortEnd - sortStart).count());
        }
#endif

        // ------------------------------------------------------------------------------------
        // Шаг 3: Draw — Vertex batching. Flush на смене zOrder или textureId.
        // ------------------------------------------------------------------------------------

#if defined(SFML1_PROFILE)
        const auto drawPhaseStart = std::chrono::steady_clock::now();
#endif

        std::size_t vertexWrite = 0;

        sf::RenderStates states;

        core::resources::ids::TextureID currentTextureId = mKeys.front().textureId;
        float currentZ = mKeys.front().zOrder;
        // Lazy per-frame cache
        const TextureCacheEntry& firstEntry = getTextureEntryCached(currentTextureId);
        const sf::Texture* currentTexture = firstEntry.texture;
        states.texture = currentTexture;

        std::size_t batchDrawCalls = 0;

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        const auto trackUniqueTexturePtr = [&](const sf::Texture* tex) {
            for (const sf::Texture* existing : mUniqueTexturePointers) {
                if (existing == tex) {
                    return;
                }
            }
            mUniqueTexturePointers.push_back(tex);
        };
        trackUniqueTexturePtr(currentTexture);
#endif

        const auto flush = [&]() {
            if (vertexWrite == 0) {
                return;
            }

#if defined(SFML1_PROFILE)
            const auto drawStart = std::chrono::steady_clock::now();
#endif

            window.draw(mVertexBuffer.get(), vertexWrite, sf::PrimitiveType::Triangles, states);

#if defined(SFML1_PROFILE)
            const auto drawEnd = std::chrono::steady_clock::now();
            drawUs += static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(drawEnd - drawStart).count());
#endif

            vertexWrite = 0;
            ++batchDrawCalls;
        };

        for (const auto& key : mKeys) {
            const RenderPacket& packet = mPackets[key.packetIndex];

            const bool zChanged = floatNotEqual(key.zOrder, currentZ);
            const bool textureChanged =
                (core::resources::ids::toUnderlying(key.textureId) !=
                 core::resources::ids::toUnderlying(currentTextureId));

            if (zChanged || textureChanged) {
                flush();

                currentZ = key.zOrder;

                if (textureChanged) {
                    currentTextureId = key.textureId;
                    const TextureCacheEntry& entry = getTextureEntryCached(currentTextureId);
                    currentTexture = entry.texture;
                    states.texture = currentTexture;

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
                    ++textureSwitches;
                    trackUniqueTexturePtr(currentTexture);
#endif
                }
            }

            const sf::IntRect rect = packet.rect;

            if (packet.isRotated) {
                writeSpriteTrianglesRotated(mVertexBuffer.get() + vertexWrite, packet.position,
                                            packet.origin, packet.scale, rect, packet.cachedSin,
                                            packet.cachedCos);
            } else {
                writeSpriteTriangles(mVertexBuffer.get() + vertexWrite, packet.position,
                                     packet.origin, packet.scale, rect);
            }
            vertexWrite += 6;
        }

        flush();

        // ------------------------------------------------------------------------------------
        // Диагностика (метрики) — Debug/Profile. Тайминги — только Profile.
        // ------------------------------------------------------------------------------------

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        FrameStats stats{};
        stats.totalSpriteCount = totalCandidates;
        stats.culledSpriteCount = culled;
        stats.spriteCount = mKeys.size();
        stats.vertexCount = stats.spriteCount * 6;
        stats.batchDrawCalls = batchDrawCalls;
        stats.textureSwitches = textureSwitches;
        stats.uniqueTexturePointers = mUniqueTexturePointers.size();
        stats.textureCacheSize = mTextureCache.size();
        stats.resourceLookupsThisFrame = resourceLookupsThisFrame;

#if defined(SFML1_PROFILE)
        {
            const auto drawPhaseEnd = std::chrono::steady_clock::now();
            const std::uint64_t drawPhaseTotalUs = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(drawPhaseEnd - drawPhaseStart)
                    .count());
            buildUs = (drawPhaseTotalUs > drawUs) ? (drawPhaseTotalUs - drawUs) : 0;
        }

        const auto totalEnd = std::chrono::steady_clock::now();
        const std::uint64_t totalUs = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(totalEnd - totalStart).count());

        stats.cpuTotalUs = totalUs;
        stats.cpuDrawUs = drawUs;
        stats.cpuGatherUs = gatherUs;
        stats.cpuSortUs = sortUs;
        stats.cpuBuildUs = buildUs;
#else
        stats.cpuTotalUs = 0;
        stats.cpuDrawUs = 0;
        stats.cpuGatherUs = 0;
        stats.cpuSortUs = 0;
        stats.cpuBuildUs = 0;
#endif

        mLastStats = stats;
#endif

#if !defined(NDEBUG)
        // Редкий лог (DEV-only): раз в 60 кадров, только при изменении visible count.
        ++mDebugFrameCount;

        if (mDebugFrameCount == 1 || mDebugFrameCount % 60 == 0) {
            const std::size_t visibleEntityCount = mKeys.size();
            if (visibleEntityCount != mDebugLastLoggedCount) {
                LOG_DEBUG(core::log::cat::ECS,
                          "RenderSystem: {} total entities, {} visible, {} draw-calls (batched), "
                          "frame {}",
                          world.aliveEntityCount(), visibleEntityCount, batchDrawCalls,
                          mDebugFrameCount);
                mDebugLastLoggedCount = visibleEntityCount;
            }
        }
#endif
    }

} // namespace core::ecs