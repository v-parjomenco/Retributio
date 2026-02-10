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

#include "core/ecs/components/spatial_id_component.h"
#include "core/ecs/components/sprite_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/world.h"
#include "core/log/log_macros.h"
#include "core/resources/resource_manager.h" // resident-only access (expectTextureResident)
#include "core/spatial/aabb2.h"
#include "core/spatial/spatial_index_v2.h"
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
                              const sf::Vector2f& origin, const sf::Vector2f& scale,
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

    const RenderSystem::FrameStats& RenderSystem::getLastFrameStatsRef() const noexcept {
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        return mLastStats;
#else
        static const FrameStats kEmpty{};
        return kEmpty;
#endif
    }

    void RenderSystem::bind(const core::spatial::SpatialIndexV2Sliding* spatialIndex,
                            std::span<const Entity> entitiesBySpatialId,
                            const std::size_t maxVisibleSprites,
                            const core::resources::ResourceManager* resources) {
        const bool hasIndex = (spatialIndex != nullptr);
        const bool hasRes = (resources != nullptr);
        const bool hasMapping = (!entitiesBySpatialId.empty());
        const bool hasVisibleCap = (maxVisibleSprites > 0);

        // Контракт: либо оба nullptr (unbind), либо оба валидны.
        if (hasIndex != hasRes) {
            LOG_PANIC(core::log::cat::ECS,
                      "RenderSystem::bind: partial bind is forbidden "
                      "(spatialIndex={}, resources={})",
                      static_cast<const void*>(spatialIndex),
                      static_cast<const void*>(resources));
        }
        if (hasIndex != hasMapping) {
            LOG_PANIC(core::log::cat::ECS,
                      "RenderSystem::bind: spatial mapping mismatch (spatialIndex={}, "
                      "mappingSize={})",
                      static_cast<const void*>(spatialIndex), entitiesBySpatialId.size());
        }
        if (hasIndex != hasVisibleCap) {
            LOG_PANIC(core::log::cat::ECS,
                      "RenderSystem::bind: invalid maxVisibleSprites (value={}, spatialIndex={})",
                      maxVisibleSprites, static_cast<const void*>(spatialIndex));
        }

        mSpatialIndex = spatialIndex;
        mResources = resources;
        mEntitiesBySpatialId = entitiesBySpatialId.data();
        mEntitiesBySpatialIdSize = entitiesBySpatialId.size();
        mMaxVisibleSprites = maxVisibleSprites;

        // Unbind: bind(nullptr, nullptr).
        if (mResources == nullptr) {
            mFrameTexturePtr.clear();
            mFrameTextureStamp.clear();
            mFrameId = 0;
            mCachedResourceGen = 0;
            mQueryBuffer.clear();
            mVisibleIds.clear();
            mVisibleCount = 0;
            mMaxVisibleSprites = 0;
            mEntitiesBySpatialId = nullptr;
            mEntitiesBySpatialIdSize = 0;

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
            mUniqueTexturesThisFrame = 0;
#endif
            return;
        }

        const std::size_t count = mResources->registry().textureCount();
        if (count == 0) {
            LOG_PANIC(core::log::cat::ECS,
                      "RenderSystem::bind: empty texture registry (textureCount == 0)");
        }

        const auto fallback = mResources->missingTextureKey();
        if (!fallback.valid()) {
            LOG_PANIC(core::log::cat::ECS,
                      "RenderSystem::bind: invalid missingTextureKey (registry misconfigured)");
        }

        // valid() гарантирует безопасный вызов index().
        const std::uint32_t fallbackIndex = fallback.index();
        if (fallbackIndex >= count) {
            LOG_PANIC(core::log::cat::ECS,
                      "RenderSystem::bind: missingTextureKey index is out of bounds "
                      "(idx={}, count={})",
                      fallbackIndex,
                      count);
        }

        resizeEpochArrays(count);
        mCachedResourceGen = mResources->cacheGeneration();

        // Prewarm buffers to fixed caps (no hot-path growth).
        mQueryBuffer.assign(mMaxVisibleSprites, core::spatial::EntityId32{0});
        mVisibleIds.clear();
        mVisibleIds.reserve(mMaxVisibleSprites);
        mVisibleCount = 0;

        if (mKeys.capacity() < mMaxVisibleSprites) {
            mKeys.reserve(mMaxVisibleSprites);
        }
        if (mPackets.capacity() < mMaxVisibleSprites) {
            mPackets.reserve(mMaxVisibleSprites);
        }

        const std::size_t maxVertices = mMaxVisibleSprites * 6u;
        if (maxVertices > 0) {
            mVertexBuffer = std::make_unique_for_overwrite<sf::Vertex[]>(maxVertices);
            mVertexBufferCapacity = maxVertices;
        } else {
            mVertexBuffer.reset();
            mVertexBufferCapacity = 0;
        }
    }

    void RenderSystem::resizeEpochArrays(const std::size_t textureCount) {
        // Без лишних аллокаций: если размер не менялся — только clear-by-fill.
        if (mFrameTexturePtr.size() != textureCount) {
            mFrameTexturePtr.assign(textureCount, nullptr);
        } else {
            std::fill(mFrameTexturePtr.begin(), mFrameTexturePtr.end(), nullptr);
        }

        if (mFrameTextureStamp.size() != textureCount) {
            mFrameTextureStamp.assign(textureCount, 0u);
        } else {
            std::fill(mFrameTextureStamp.begin(), mFrameTextureStamp.end(), 0u);
        }
        // После resizeEpochArrays следующий beginFrame() даст mFrameId == 1.
        mFrameId = 0;
    }

    void RenderSystem::beginFrame() noexcept {
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        mUniqueTexturesThisFrame = 0;
#endif

        // Инвалидируем per-frame pointer cache, если ResourceManager сбросил/перестроил кэши.
        if (mResources != nullptr) {
            const std::uint32_t gen = mResources->cacheGeneration();
            if (gen != mCachedResourceGen) {
                std::fill(mFrameTexturePtr.begin(), mFrameTexturePtr.end(), nullptr);
                std::fill(mFrameTextureStamp.begin(), mFrameTextureStamp.end(), 0u);
                mFrameId = 0;
                mCachedResourceGen = gen;
            }
        }
        ++mFrameId;

        if (mFrameId == 0) {
            std::fill(mFrameTextureStamp.begin(), mFrameTextureStamp.end(), 0u);
            mFrameId = 1;
        }

        if (mSpatialIndex != nullptr && mSpatialIndex->marksClearRequired()) {
            mSpatialIndex->clearMarksTable();
        }
    }

    const sf::Texture* RenderSystem::getTextureCached(core::resources::TextureKey key,
                                                      std::size_t* resourceLookupsThisFrame) {
        assert(mResources != nullptr);

        // Контракт: epoch-массивы должны быть подготовлены в bind().
        assert(!mFrameTexturePtr.empty() && mFrameTexturePtr.size() == mFrameTextureStamp.size());

        if (!key.valid()) {
            key = mResources->missingTextureKey();
        }

        std::uint32_t index = key.index();
        if (index >= mFrameTexturePtr.size()) {
            key = mResources->missingTextureKey();
            index = key.index();

            // Контракт bind(): missingTextureKey.index() < textureCount.
            assert(index < mFrameTexturePtr.size() &&
                   "RenderSystem: missingTextureKey is out of bounds "
                   "(bind() not called or registry broken)");
        }

        if (mFrameTextureStamp[index] != mFrameId) {
            mFrameTexturePtr[index] = &mResources->expectTextureResident(key).get();
            mFrameTextureStamp[index] = mFrameId;

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
            ++mUniqueTexturesThisFrame;
#endif
            if (resourceLookupsThisFrame != nullptr) {
                ++(*resourceLookupsThisFrame);
            }
        }

        return mFrameTexturePtr[index];
    }

    // --------------------------------------------------------------------------------------------
    // Примечание:
    //  - render() намеренно НЕ noexcept: sf::RenderWindow::draw() не гарантирует noexcept.
    //  - Исключения (если они вообще возможны в текущей конфигурации) пробрасываются в Game layer.
    // --------------------------------------------------------------------------------------------

    void RenderSystem::render(World& world, sf::RenderWindow& window) {
        // Release: trust-on-read (bind контракт обязателен). Debug/Profile: ловим misuse заранее.
        assert(window.getView().getRotation() == sf::Angle::Zero &&
               "RenderSystem: view rotation is not supported (view.getRotation() must be 0).");
        assert(mSpatialIndex != nullptr && "RenderSystem: bind() не вызван — mSpatialIndex null");
        assert(mResources != nullptr && "RenderSystem: bind() не вызван — mResources null");
        assert(mEntitiesBySpatialId != nullptr && mEntitiesBySpatialIdSize > 0 &&
               "RenderSystem: spatial mapping not bound");
        assert(mFrameTexturePtr.size() == mResources->registry().textureCount() &&
               "RenderSystem: epoch cache size mismatch (call bind() after registry reload).");
        assert(mFrameTextureStamp.size() == mResources->registry().textureCount() &&
               "RenderSystem: epoch stamp size mismatch (call bind() after registry reload).");

        // View-culling в world-space координатах текущего view.
        const core::spatial::Aabb2 viewAabb = makeViewAabb(window.getView());

#if defined(SFML1_PROFILE)
        const auto totalStart = std::chrono::steady_clock::now();
        std::uint64_t drawUs = 0;
#endif

        std::size_t resourceLookupsThisFrame = 0;

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        std::size_t totalCandidates = 0;
        std::size_t culled = 0;
        std::size_t textureSwitches = 0;
        std::size_t mapNull = 0;
        std::size_t missingComponents = 0;
        std::size_t fineCullFail = 0;
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

        if (mQueryBuffer.empty()) {
            LOG_PANIC(core::log::cat::ECS,
                      "RenderSystem: query buffer not prewarmed (bind() not called)");
        }

        mKeys.clear();
        mPackets.clear();
        mVisibleIds.clear();
        beginFrame();

        const std::span<core::spatial::EntityId32> outIds(mQueryBuffer);
        const std::size_t idCount = mSpatialIndex->queryFast(viewAabb, outIds);
        if (idCount == outIds.size()) {
            LOG_ERROR(core::log::cat::ECS,
                      "RenderSystem: SpatialIndexV2 query truncated (capacity={})", outIds.size());
        }
        mVisibleCount = idCount;
        mLastVisibleCount = idCount;

        // ----------------------------------------------------------------------------------------
        // КРИТИЧНАЯ ОПТИМИЗАЦИЯ: Vertex buffer без инициализации.
        // std::vector<sf::Vertex>::resize(N) зануляет ~7MB при 50k sprites.
        // std::make_unique_for_overwrite<sf::Vertex[]> (C++20) = zero init cost.
        // ----------------------------------------------------------------------------------------
        const std::size_t maxVertices = mVisibleCount * 6;
        if (maxVertices > mVertexBufferCapacity) {
            LOG_PANIC(core::log::cat::ECS,
                      "RenderSystem: vertex buffer overflow (visible={}, capacity={})",
                      mVisibleCount, mVertexBufferCapacity / 6u);
        }

        auto ecsView = world.view<TransformComponent, SpriteComponent, SpatialIdComponent>();

        // ----------------------------------------------------------------------------------------
        // Детерминизм: drawOrdinal tie-breaker
        // ----------------------------------------------------------------------------------------
        //  - drawOrdinal = монотонный per-frame счётчик (0..N-1)
        //  - Каждый спрайт получает уникальный tieBreak в порядке gather
        //  - Компаратор (zOrder, texture, tieBreak) → строгий total order
        //
        // ПРЕИМУЩЕСТВА:
        //  - Нет коллизий (каждый спрайт = уникальный drawOrdinal)
        //  - Детерминизм (порядок gather = порядок отрисовки при равных zOrder/texture)
        //  - ZERO COST (инкремент uint32 = 1 CPU cycle)
        //  - Компактно (uint32, не uint64 → cache-friendly, 16 bytes RenderKey)
        //
        // ДЕТЕРМИНИЗМ ГАРАНТИИ:
        //  - Порядок gather определяется порядком итерации по mQueryBuffer
        //  - mQueryBuffer заполняется SpatialIndex::queryFast() детерминированно
        //  - SpatialIndex детерминирован (фиксированный chunkSize, cellSize, windowOrigin)
        //  → drawOrdinal детерминирован → sort детерминирован → рендер детерминирован
        std::uint32_t drawOrdinal = 0;

        // Итерация по mQueryBuffer (уже отфильтровано SpatialIndexV2).
        // Если число видимых сущностей будет постоянно превышать 50k, поменять на:
        //  packed iteration через view.each() + inline culling.

        for (std::size_t i = 0; i < mVisibleCount; ++i) {
            const core::spatial::EntityId32 id = mQueryBuffer[i];
            assert(id < mEntitiesBySpatialIdSize &&
                   "RenderSystem: SpatialId32 out of range (mapping)");
            const Entity entity = mEntitiesBySpatialId[id];
            if (entity == core::ecs::NullEntity) {
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
                ++mapNull;
#endif
                continue;
            }
            if (!ecsView.contains(entity)) {
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
                ++missingComponents;
#endif
                continue;
            }
            assert(ecsView.contains(entity) && "RenderSystem: SpatialIndex вернул entity без "
                                               "(Transform, Sprite, SpatialId)");
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
            ++totalCandidates;
#endif
            const auto& spr = ecsView.get<SpriteComponent>(entity);
            const auto& tr = ecsView.get<TransformComponent>(entity);
            const auto& sh = ecsView.get<SpatialIdComponent>(entity);

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
            assert((rect.size.x > 0 && rect.size.y > 0) &&
                   "RenderSystem: textureRect должен иметь положительный размер");

            // SpatialIndex возвращает cell-level кандидатов.
            // Fine AABB culling обязателен для pixel-accurate visibility.
            // Entity в ячейке может НЕ пересекаться с viewAabb, даже если ячейка пересекается.
            if (!core::spatial::intersectsInclusive(sh.lastAabb, viewAabb)) {
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
                ++culled;
                ++fineCullFail;
#endif
                continue;
            }

            mVisibleIds.push_back(id);

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
                                      .texture = spr.texture,
                                      .tieBreak = drawOrdinal++,
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
            stats.textureCacheSize = mFrameTexturePtr.size();
            stats.resourceLookupsThisFrame = resourceLookupsThisFrame;
            stats.renderMapNull = mapNull;
            stats.renderMissingComponents = missingComponents;
            stats.renderFineCullFail = fineCullFail;
            stats.renderDrawn = 0;
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
            const auto& qstats = mSpatialIndex->debugLastQueryStatsRef();
            stats.spatialEntriesScanned = qstats.entriesScanned;
            stats.spatialQueryUnique = qstats.uniqueAdded;
            stats.spatialDupHits = qstats.dupHits;
            stats.spatialCellsVisited = qstats.cellVisits;
            stats.spatialChunksVisited = qstats.chunksVisited;
            stats.spatialChunksSkipped = qstats.chunksSkippedNonLoaded;
            stats.spatialOutTruncated = qstats.outTruncated;
#endif
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

        std::sort(mKeys.begin(), mKeys.end(), [](const RenderKey& a, const RenderKey& b) {
            if (a.zOrder < b.zOrder) {
                return true;
            }
            if (b.zOrder < a.zOrder) {
                return false;
            }

            const auto aTex = a.texture.sortKey();
            const auto bTex = b.texture.sortKey();
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

        // ----------------------------------------------------------------------------------------
        // Шаг 3: Draw — Vertex batching. Flush на смене zOrder или texture.
        // ----------------------------------------------------------------------------------------

#if defined(SFML1_PROFILE)
        const auto drawPhaseStart = std::chrono::steady_clock::now();
#endif

        std::size_t vertexWrite = 0;

        sf::RenderStates states{}; // value-init: детерминированные дефолты

        core::resources::TextureKey currentTexture = mKeys.front().texture;
        float currentZ = mKeys.front().zOrder;

        const sf::Texture* currentTexturePtr =
            getTextureCached(currentTexture, &resourceLookupsThisFrame);
        states.texture = currentTexturePtr;

        std::size_t batchDrawCalls = 0;

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
            const bool textureChanged = (key.texture != currentTexture);

            if (zChanged || textureChanged) {
                flush();

                currentZ = key.zOrder;

                if (textureChanged) {
                    currentTexture = key.texture;
                    currentTexturePtr = getTextureCached(currentTexture, &resourceLookupsThisFrame);
                    states.texture = currentTexturePtr;

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
                    ++textureSwitches;
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
        stats.uniqueTexturePointers = mUniqueTexturesThisFrame;
        stats.textureCacheSize = mFrameTexturePtr.size();
        stats.resourceLookupsThisFrame = resourceLookupsThisFrame;
        stats.renderMapNull = mapNull;
        stats.renderMissingComponents = missingComponents;
        stats.renderFineCullFail = fineCullFail;
        stats.renderDrawn = mKeys.size();
#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        {
            const auto& qstats = mSpatialIndex->debugLastQueryStatsRef();
            stats.spatialEntriesScanned = qstats.entriesScanned;
            stats.spatialQueryUnique = qstats.uniqueAdded;
            stats.spatialDupHits = qstats.dupHits;
            stats.spatialCellsVisited = qstats.cellVisits;
            stats.spatialChunksVisited = qstats.chunksVisited;
            stats.spatialChunksSkipped = qstats.chunksSkippedNonLoaded;
            stats.spatialOutTruncated = qstats.outTruncated;
        }
#endif

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