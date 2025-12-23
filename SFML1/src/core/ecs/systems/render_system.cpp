#include "pch.h"

#include "core/ecs/systems/render_system.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
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

    struct Aabb2 {
        float minX = 0.f;
        float minY = 0.f;
        float maxX = 0.f;
        float maxY = 0.f;
    };

    [[nodiscard]] bool isFiniteVec2(const sf::Vector2f& v) noexcept {
        return std::isfinite(v.x) && std::isfinite(v.y);
    }

    [[nodiscard]] bool intersectsInclusive(const Aabb2& a, const Aabb2& b) noexcept {
        // Инклюзивное пересечение (касание границ считаем видимым).
        return (a.maxX >= b.minX) && (a.minX <= b.maxX) &&
               (a.maxY >= b.minY) && (a.minY <= b.maxY);
    }

    [[nodiscard]] Aabb2 makeViewAabb(const sf::View& view) noexcept {
        // ВАЖНО: это AABB текущего view в world-space. Ротацию view не учитываем
        // (в SkyGuard/4X это обычно не используется; если появится — нужен OBB/convex test).
        const sf::Vector2f size = view.getSize();
        const sf::Vector2f center = view.getCenter();
        const sf::Vector2f topLeft{center.x - size.x * 0.5f, center.y - size.y * 0.5f};

        Aabb2 out{};
        out.minX = topLeft.x;
        out.minY = topLeft.y;
        out.maxX = topLeft.x + size.x;
        out.maxY = topLeft.y + size.y;
        return out;
    }

    [[nodiscard]] bool floatNotEqual(const float a, const float b) noexcept {
        // Строго соответствует логике сортировки: "не равны" == (a<b || b<a)
        // (заодно не полагаемся на != для float).
        return (a < b) || (b < a);
    }

    [[nodiscard]] bool hasExplicitRect(const sf::IntRect& r) noexcept {
        // Политика: (0,0) => full texture. Иначе rect должен быть задан явно (оба != 0).
        return (r.size.x != 0) && (r.size.y != 0);
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

        out[0].position = p0; out[0].color = color; out[0].texCoords = t0;
        out[1].position = p1; out[1].color = color; out[1].texCoords = t1;
        out[2].position = p2; out[2].color = color; out[2].texCoords = t2;
        out[3].position = p0; out[3].color = color; out[3].texCoords = t0;
        out[4].position = p2; out[4].color = color; out[4].texCoords = t2;
        out[5].position = p3; out[5].color = color; out[5].texCoords = t3;
    }

    [[nodiscard]] Aabb2 computeSpriteAabbNoRotation(const sf::Vector2f& position,
                                                    const sf::Vector2f& origin,
                                                    const sf::Vector2f& scale,
                                                    const sf::IntRect& rect) noexcept {
        // Должно соответствовать appendSpriteTriangles(): те же локальные точки и тот же порядок
        // transform (origin -> scale -> translate). Так culling совпадает с реальной геометрией.
        const float w = static_cast<float>(rect.size.x);
        const float h = static_cast<float>(rect.size.y);

        const float ox = origin.x;
        const float oy = origin.y;
        const float sx = scale.x;
        const float sy = scale.y;
        const float px = position.x;
        const float py = position.y;

        const float x0 = ((-ox)     * sx) + px;
        const float y0 = ((-oy)     * sy) + py;
        const float x1 = ((w - ox)  * sx) + px;
        const float y1 = ((-oy)     * sy) + py;
        const float x2 = ((w - ox)  * sx) + px;
        const float y2 = ((h - oy)  * sy) + py;
        const float x3 = ((-ox)     * sx) + px;
        const float y3 = ((h - oy)  * sy) + py;

        const float minX = std::min(std::min(x0, x1), std::min(x2, x3));
        const float maxX = std::max(std::max(x0, x1), std::max(x2, x3));
        const float minY = std::min(std::min(y0, y1), std::min(y2, y3));
        const float maxY = std::max(std::max(y0, y1), std::max(y2, y3));

        Aabb2 out{};
        out.minX = minX;
        out.minY = minY;
        out.maxX = maxX;
        out.maxY = maxY;
        return out;
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

    void RenderSystem::render(World& world, sf::RenderWindow& window) {

        // view-culling работаем в world-space координатах текущего view.
        const Aabb2 viewAabb = makeViewAabb(window.getView());

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

        // ----------------------------------------------------------------------------------------
        // Шаг 1: Gather + Culling — собираем ключи только для видимых спрайтов.
        // ----------------------------------------------------------------------------------------

        mKeys.clear();
        mTextureCache.clear();

        auto ecsView = world.view<TransformComponent, SpriteComponent>();
        mKeys.reserve(ecsView.size_hint());

        // Per-frame cache: TextureID -> (sf::Texture*, fullRect).
        // Должен покрывать И explicitRect кейсы тоже.
        const auto findTextureEntry = [&](core::resources::ids::TextureID id)
            -> TextureCacheEntry* {
            for (auto& e : mTextureCache) {
                if (e.id == id) {
                    return &e;
                }
            }
            return nullptr;
        };

        const auto getTextureEntryCached = [&](core::resources::ids::TextureID id)
            -> const TextureCacheEntry& {
            if (TextureCacheEntry* existing = findTextureEntry(id)) {
                return *existing;
            }

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
            ++resourceLookupsThisFrame;
#endif

            const sf::Texture& tex = mResources->getTexture(id).get();
            TextureCacheEntry entry{};
            entry.id = id;
            entry.texture = &tex;
            entry.fullRect = makeFullRect(tex);
            mTextureCache.push_back(entry);
            return mTextureCache.back();
        };

        for (const Entity entity : ecsView) {
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
            ++totalCandidates;
#endif
            const auto& spr = ecsView.get<SpriteComponent>(entity);
            const auto& tr = ecsView.get<TransformComponent>(entity);

            const bool dataFinite =
                isFiniteVec2(tr.position) &&
                isFiniteVec2(spr.origin) &&
                isFiniteVec2(spr.scale) &&
                std::isfinite(spr.zOrder);

#if !defined(NDEBUG)
            // Это engine-level инвариант: NaN/Inf сюда попадать не должны.
            assert(dataFinite && "RenderSystem: non-finite Transform/Sprite data detected.");
#endif
            if (!dataFinite) {
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
                ++culled;
#endif
                continue;
            }

            const TextureCacheEntry* texEntry = nullptr;
            sf::IntRect rect{};
            if (hasExplicitRect(spr.textureRect)) {
#if !defined(NDEBUG)
                // Политика: если rect задан, он должен быть положительным.
                assert((spr.textureRect.size.x > 0 && spr.textureRect.size.y > 0) &&
                       "SpriteComponent.textureRect must be (0,0)"
                       "for full texture or positive sizes.");
#endif
                rect = spr.textureRect;
            } else {
                texEntry = &getTextureEntryCached(spr.textureId);
                rect = texEntry->fullRect;
            }

            const Aabb2 spriteAabb =
                computeSpriteAabbNoRotation(tr.position, spr.origin, spr.scale, rect);

            if (!intersectsInclusive(spriteAabb, viewAabb)) {
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
                ++culled;
#endif
                continue;
            }

            // ВАЖНО: в стресс-сцене rect обычно explicit, но текстура всё равно нужна в draw-фазе.
            // Резолвим TextureID -> sf::Texture* максимум один раз на кадр для каждого ID.
            if (texEntry == nullptr) {
                (void)getTextureEntryCached(spr.textureId);
            }

            mKeys.push_back(RenderKey{
                .zOrder = spr.zOrder,
                .textureId = spr.textureId,
                .entity = entity
            });
        }

#if defined(SFML1_PROFILE)
        {
            const auto gatherEnd = std::chrono::steady_clock::now();
            gatherUs = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                gatherEnd - gatherStart).count());
        }
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

        // ----------------------------------------------------------------------------------------
        // Шаг 2: Sort — детерминированно и batching-friendly.
        // ----------------------------------------------------------------------------------------

#if defined(SFML1_PROFILE)
        const auto sortStart = std::chrono::steady_clock::now();
#endif

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

#if defined(SFML1_PROFILE)
        {
            const auto sortEnd = std::chrono::steady_clock::now();
            sortUs = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                sortEnd - sortStart).count());
        }
#endif

        // ----------------------------------------------------------------------------------------
        // Шаг 3: Draw — Vertex batching (CPU). Flush на смене zOrder или textureId.
        // ----------------------------------------------------------------------------------------

#if defined(SFML1_PROFILE)
        const auto drawPhaseStart = std::chrono::steady_clock::now();
#endif

        const std::size_t maxVertices = mKeys.size() * 6;
        if (mVertices.size() < maxVertices) {
            mVertices.resize(maxVertices);
        }
        std::size_t vertexWrite = 0;

        sf::RenderStates states;

        core::resources::ids::TextureID currentTextureId = mKeys.front().textureId;
        float currentZ = mKeys.front().zOrder;

        const auto resolveTextureFromCache = [&](core::resources::ids::TextureID id)
            -> const TextureCacheEntry& {
            if (TextureCacheEntry* e = findTextureEntry(id)) {
                return *e;
            }
            // На практике не должно случаться (мы кэшируем в gather), но держим safe fallback.
            return getTextureEntryCached(id);
        };
        
        const TextureCacheEntry& firstEntry = resolveTextureFromCache(currentTextureId);
        const sf::Texture* currentTexture = firstEntry.texture;
        states.texture = currentTexture;

        sf::IntRect fullRect = firstEntry.fullRect;

        std::size_t batchDrawCalls = 0;

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        const auto trackUniqueTexturePtr = [&](const sf::Texture* tex) {
            // Линейный поиск: ожидаемо маленькое число уникальных текстур (десятки), overhead минимальный.
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

            window.draw(mVertices.data(),
                        vertexWrite,
                        sf::PrimitiveType::Triangles,
                        states);

#if defined(SFML1_PROFILE)
            const auto drawEnd = std::chrono::steady_clock::now();
            drawUs += static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                drawEnd - drawStart).count());
#endif

            vertexWrite = 0;
            ++batchDrawCalls;
        };

        for (const auto& key : mKeys) {
            const Entity entity = key.entity;

            const auto& transform = ecsView.get<TransformComponent>(entity);
            const auto& spr = ecsView.get<SpriteComponent>(entity);

            const bool zChanged = floatNotEqual(key.zOrder, currentZ);
            const bool textureChanged = (key.textureId != currentTextureId);

            if (zChanged || textureChanged) {
                flush();

                currentZ = key.zOrder;

                if (textureChanged) {
                    currentTextureId = key.textureId;
                    const TextureCacheEntry& entry = resolveTextureFromCache(currentTextureId);
                    currentTexture = entry.texture;
                    states.texture = currentTexture;

                    fullRect = entry.fullRect;

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
                    ++textureSwitches;
                    trackUniqueTexturePtr(currentTexture);
#endif
                }
            }

            const sf::IntRect rect =
                (hasExplicitRect(spr.textureRect) &&
                 (spr.textureRect.size.x > 0 && spr.textureRect.size.y > 0))
                    ? spr.textureRect
                    : fullRect;

            writeSpriteTriangles(mVertices.data() + vertexWrite,
                                 transform.position,
                                 spr.origin,
                                 spr.scale,
                                 rect);
            vertexWrite += 6;
        }

        flush();

        // ----------------------------------------------------------------------------------------
        // Диагностика (метрики) — Debug/Profile. Тайминги — только Profile.
        // ----------------------------------------------------------------------------------------

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
                std::chrono::duration_cast<std::chrono::microseconds>(drawPhaseEnd - drawPhaseStart).count());
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
        // Редкий лог (DEV-only): не спамим, только раз в ~60 кадров и только при изменении count.
        static int frameCount = 0;
        static std::size_t lastLoggedCount = static_cast<std::size_t>(-1);

        ++frameCount;

        if (frameCount == 1 || frameCount % 60 == 0) {
            const std::size_t entityCount = mKeys.size();
            if (entityCount != lastLoggedCount) {
                LOG_DEBUG(core::log::cat::ECS,
                          "RenderSystem: {} visible, {} draw-calls (batched), frame {}",
                          entityCount, batchDrawCalls, frameCount);
                lastLoggedCount = entityCount;
            }
        }
#endif
    }

} // namespace core::ecs