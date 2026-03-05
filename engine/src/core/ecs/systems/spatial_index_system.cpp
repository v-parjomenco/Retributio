#include "pch.h"

#include "core/ecs/systems/spatial_index_system.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include "core/ecs/components/spatial_dirty_tag.h"
#include "core/ecs/components/spatial_id_component.h"
#include "core/ecs/components/spatial_streamed_out_tag.h"
#include "core/ecs/components/sprite_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/render/sprite_bounds.h"
#include "core/ecs/world.h"
#include "core/log/log_macros.h"
#include "core/utils/math_constants.h"

namespace {

    [[nodiscard]] core::spatial::Aabb2 computeEntityAabb(
        const core::ecs::TransformComponent& tr,
        const core::ecs::SpriteComponent& sp) noexcept {

        const float rotationDegrees = tr.rotationDegrees;
        if (rotationDegrees == 0.f) {
            return core::ecs::render::computeSpriteAabbNoRotation(
                tr.position, sp.origin, sp.scale, sp.textureRect);
        }

        const float radians = rotationDegrees * core::utils::kDegToRad;
        const float cachedSin = std::sin(radians);
        const float cachedCos = std::cos(radians);
        return core::ecs::render::computeSpriteAabbRotated(
            tr.position, sp.origin, sp.scale, sp.textureRect, cachedSin, cachedCos);
    }

} // namespace

namespace core::ecs {

    void SpatialIndexSystem::beginFrameRead() noexcept {
        mIndex.beginFrameRead();
    }

    SpatialIndexSystem::SpatialIndexSystem(const SpatialIndexSystemConfig& config) noexcept
        : mIndex() {
        assert(config.maxEntityId > 0 && "SpatialIndexSystem: maxEntityId must be > 0");
        assert(config.index.maxEntityId > 0 && "SpatialIndexSystem: index.maxEntityId must be > 0");
        assert(config.index.cellSizeWorld > 0 && "SpatialIndexSystem: cellSizeWorld must be > 0");
        assert(config.index.chunkSizeWorld > 0 && "SpatialIndexSystem: chunkSizeWorld must be > 0");
        assert(config.index.maxEntityId == config.maxEntityId &&
               "SpatialIndexSystem: index.maxEntityId must match maxEntityId");

        mIndex.init(config.index, config.storage);

        // Инварианты ID-пула (SpatialId32):
        //  - id==0: "нет привязки".
        //  - mEntityBySpatialId[id] == NullEntity <=> id свободен (может быть в free-list).
        //  - Если у сущности есть SpatialIdComponent{id!=0}, то mEntityBySpatialId[id] == entity.
        //  - releaseSpatialId(id) разрешён ТОЛЬКО если mapping[id] != NullEntity (fail-fast).
        mEntityBySpatialId.clear();
        mEntityBySpatialId.resize(static_cast<std::size_t>(config.maxEntityId) + 1u,
                                  core::ecs::NullEntity);

        mFreeSpatialIds.clear();
        mFreeSpatialIds.reserve(static_cast<std::size_t>(config.maxEntityId) / 16u);

        mNextSpatialId = 1u;
        mDeterminismEnabled = config.determinismEnabled;
    }

    // ============================================================================================
    // update(): orchestrator only — registration of new entities + dispatch dirty path + tag clear.
    // ============================================================================================

    void SpatialIndexSystem::update(World& world, float) {

        ensureDestroyConnection(world);

        auto& registry = world.registry();

        // ----- Phase 1: register new entities (no SpatialIdComponent yet) -----------------------

        auto newView = registry.view<TransformComponent, SpriteComponent>(
                    entt::exclude<SpatialIdComponent, SpatialStreamedOutTag>);

        for (auto [entity, transform, sprite] : newView.each()) {
#if !defined(NDEBUG)
            // Двойная страховка от будущих регрессий: streamed-out сущности не должны
            // попадать в регистрацию. В Release/Profile нулевой overhead.
            assert(!registry.any_of<SpatialStreamedOutTag>(entity) &&
                   "SpatialIndexSystem: streamed-out entity must not be registered");
#endif
            assert(core::ecs::render::hasExplicitRect(sprite.textureRect) &&
                   "SpatialIndexSystem: SpriteComponent.textureRect must be explicit");

            const core::spatial::Aabb2 aabb = computeEntityAabb(transform, sprite);

            const core::spatial::EntityId32 id = allocateSpatialId();
            if (id == 0) {
                LOG_PANIC(core::log::cat::ECS,
                          "SpatialIndexSystem: SpatialId32 pool exhausted (maxEntityId={})",
                          mEntityBySpatialId.size() - 1u);
            }

            // КРИТИЧЕСКИЙ КОНТРАКТ (state machine):
            //  allocateSpatialId() возвращает "зарезервированный" id, но сам allocate НЕ знает
            //  entity, поэтому mapping должен быть установлен СРАЗУ ЖЕ, до любых операций, которые
            //  могут привести к releaseSpatialId(id) (ошибка регистрации, ранний выход и т.п.).
            //
            // Это гарантирует, что releaseSpatialId(id) остаётся строгим fail-fast:
            // release без mapping == контрактная ошибка (или реальный double-free).
            setMapping(id, entity);

            const core::spatial::WriteResult writeResult = mIndex.registerEntity(id, aabb);
            if (writeResult == core::spatial::WriteResult::Rejected) {
                // Диагностика только на фейле: нулевой cost на успешном пути.
                const bool isFinite = std::isfinite(aabb.minX) && std::isfinite(aabb.minY) &&
                                      std::isfinite(aabb.maxX) && std::isfinite(aabb.maxY);

                const std::int32_t chunkSize = mIndex.chunkSizeWorld();

                const core::spatial::ChunkCoord minChunk = core::spatial::worldToChunk(
                    core::spatial::WorldPosf{aabb.minX, aabb.minY}, chunkSize);
                const core::spatial::ChunkCoord maxChunk = core::spatial::worldToChunk(
                    core::spatial::WorldPosf{aabb.maxX, aabb.maxY}, chunkSize);

                core::spatial::ChunkCoord firstBad{minChunk.x, minChunk.y};
                core::spatial::ResidencyState firstBadState =
                    core::spatial::ResidencyState::Unloaded;
                bool foundBad = false;

                for (std::int32_t cy = minChunk.y; cy <= maxChunk.y && !foundBad; ++cy) {
                    for (std::int32_t cx = minChunk.x; cx <= maxChunk.x; ++cx) {
                        const auto state = mIndex.chunkState({cx, cy});
                        if (state != core::spatial::ResidencyState::Loaded) {
                            firstBad = {cx, cy};
                            firstBadState = state;
                            foundBad = true;
                            break;
                        }
                    }
                }

                releaseSpatialId(id);

                const core::spatial::ChunkCoord winOrigin = mIndex.windowOrigin();
                LOG_PANIC(core::log::cat::ECS,
                          "SpatialIndexSystem: registerEntity rejected (id={}, entity={}) "
                          "AABB=({:.3f},{:.3f})..({:.3f},{:.3f}) finite={} "
                          "chunkRange=({}, {})..({}, {}) "
                          "firstNonLoaded=({}, {}) state={} "
                          "windowOrigin=({}, {}) windowSize={}x{}",
                          id, core::ecs::toUint(entity), aabb.minX, aabb.minY, aabb.maxX, aabb.maxY,
                          isFinite ? 1 : 0, minChunk.x, minChunk.y, maxChunk.x, maxChunk.y,
                          firstBad.x, firstBad.y, static_cast<int>(firstBadState), winOrigin.x,
                          winOrigin.y, mIndex.windowWidth(), mIndex.windowHeight());
            }

            // entity гарантированно не имел SpatialIdComponent (exclude<>),
            // поэтому emplace достаточно.
            // Инвариант после emplace: SpatialIdComponent.id != 0 и mapping[id] == entity.
            registry.emplace<SpatialIdComponent>(entity, SpatialIdComponent{id, aabb});
        }

        // ----- Phase 2: update dirty entities ---------------------------------------------------

        const bool hadDirty = mDeterminismEnabled
                                  ? updateDirtyDeterministic(world, registry)
                                  : updateDirtyFast(registry);

        // ВАЖНО: не удаляем SpatialDirtyTag внутри итерации view (риск инвалидации).
        // storage SpatialDirtyTag содержит только "грязные" сущности, поэтому clear() = O(dirty),
        // без аллокаций и без лишних проходов по "чистым" сущностям.
        if (hadDirty) {
            registry.clear<SpatialDirtyTag>();
        }
    }

    // ============================================================================================
    // updateDirtyDeterministic: sorted by StableId for reproducible update order.
    // Контракт: comparator работает по заранее заполненному scratch (не optional-lookup в sort).
    // ============================================================================================

    bool SpatialIndexSystem::updateDirtyDeterministic(World& world, entt::registry& registry) {
        auto dirtyView = registry.view<SpatialIdComponent,
                                       SpatialDirtyTag,
                                       TransformComponent,
                                       SpriteComponent>(
                                       entt::exclude<SpatialStreamedOutTag>);

        auto& stableIds = world.stableIds();

        const std::size_t dirtyHint = dirtyView.size_hint();
        if (mDirtyStableScratch.capacity() < dirtyHint) {
            mDirtyStableScratch.reserve(dirtyHint);
        }
        mDirtyStableScratch.clear();

        for (const Entity e : dirtyView) {
            const auto idOpt = stableIds.tryGet(e);
            if (!idOpt.has_value()) {
                LOG_PANIC(core::log::cat::ECS,
                          "SpatialIndexSystem: missing StableID in deterministic mode. "
                          "Stable IDs must be assigned on entity creation.");
            }
            mDirtyStableScratch.emplace_back(*idOpt, e);
        }

        if (mDirtyStableScratch.empty()) {
            return false;
        }

        std::sort(mDirtyStableScratch.begin(), mDirtyStableScratch.end(),
                  [](const auto& lhs, const auto& rhs) noexcept {
                      if (lhs.first < rhs.first) {
                          return true;
                      }
                      if (rhs.first < lhs.first) {
                          return false;
                      }
                      // Теоретический tie-break (StableIdService обещает уникальность).
                      return core::ecs::toUint(lhs.second) < core::ecs::toUint(rhs.second);
                  });

        for (const auto& entry : mDirtyStableScratch) {
            const Entity e = entry.second;
            auto& handleComp = registry.get<SpatialIdComponent>(e);
            const auto& tr = registry.get<TransformComponent>(e);
            const auto& sp = registry.get<SpriteComponent>(e);

            assert(core::ecs::render::hasExplicitRect(sp.textureRect) &&
                   "SpatialIndexSystem: SpriteComponent.textureRect must be explicit");

            const core::spatial::Aabb2 newAabb = computeEntityAabb(tr, sp);

            const core::spatial::WriteResult updResult =
                mIndex.updateEntity(handleComp.id, newAabb);
            if (updResult == core::spatial::WriteResult::Rejected) {
                LOG_PANIC(core::log::cat::ECS,
                          "SpatialIndexSystem: updateEntity rejected (id={})", handleComp.id);
            }
            handleComp.lastAabb = newAabb;
        }

        return true;
    }

    // ============================================================================================
    // updateDirtyFast: raw EnTT iteration order (non-deterministic, zero overhead).
    // ============================================================================================

    bool SpatialIndexSystem::updateDirtyFast(entt::registry& registry) {
        auto dirtyView = registry.view<SpatialIdComponent,
                                       SpatialDirtyTag,
                                       TransformComponent,
                                       SpriteComponent>(
                                       entt::exclude<SpatialStreamedOutTag>);

        bool hadDirty = false;

        for (auto [entity, handleComp, transform, sprite] : dirtyView.each()) {
            (void) entity;
            hadDirty = true;

            assert(core::ecs::render::hasExplicitRect(sprite.textureRect) &&
                   "SpatialIndexSystem: SpriteComponent.textureRect must be explicit");

            const core::spatial::Aabb2 newAabb = computeEntityAabb(transform, sprite);

            const core::spatial::WriteResult result =
                mIndex.updateEntity(handleComp.id, newAabb);
            if (result == core::spatial::WriteResult::Rejected) {
                LOG_PANIC(core::log::cat::ECS,
                          "SpatialIndexSystem: updateEntity rejected (id={})", handleComp.id);
            }
            handleComp.lastAabb = newAabb;
        }

        return hadDirty;
    }

    // ============================================================================================
    // Destroy callback + ID pool management (unchanged)
    // ============================================================================================

    void SpatialIndexSystem::ensureDestroyConnection(World& world) {
        if (mConnected) {
            return;
        }

        auto& registry = world.registry();
        mOnDestroyConnection = registry.on_destroy<SpatialIdComponent>().connect<
            &SpatialIndexSystem::onHandleDestroyed>(*this);
        mConnected = true;
    }

    void SpatialIndexSystem::onHandleDestroyed(entt::registry& registry, Entity entity) noexcept {
        // on_destroy<SpatialIdComponent> гарантирует, что компонент ещё доступен в callback.
        const auto& handle = registry.get<SpatialIdComponent>(entity);
        if (handle.id == 0) {
            return;
        }

#if !defined(NDEBUG)
        assert(handle.id < mEntityBySpatialId.size() &&
               "SpatialIndexSystem: SpatialId32 out of range on destroy");
        assert(mEntityBySpatialId[handle.id] == entity &&
               "SpatialIndexSystem: SpatialId32 mapping mismatch on destroy");
#else
        if (handle.id >= mEntityBySpatialId.size()) [[unlikely]] {
            LOG_PANIC(core::log::cat::ECS,
                      "SpatialIndexSystem: SpatialId32 out of range on destroy (id={})",
                      handle.id);
        }
        if (mEntityBySpatialId[handle.id] != entity) [[unlikely]] {
            LOG_PANIC(core::log::cat::ECS,
                      "SpatialIndexSystem: SpatialId32 mapping mismatch on destroy (id={})",
                      handle.id);
        }
#endif

        const bool removed = mIndex.unregisterEntity(handle.id);
        if (!removed) {
            LOG_PANIC(core::log::cat::ECS,
                      "SpatialIndexSystem: unregisterEntity rejected (id={})",
                      handle.id);
        }

        releaseSpatialId(handle.id);
    }

    core::spatial::EntityId32 SpatialIndexSystem::allocateSpatialId() {
        if (!mFreeSpatialIds.empty()) {
            const core::spatial::EntityId32 id = mFreeSpatialIds.back();
            mFreeSpatialIds.pop_back();

#if !defined(NDEBUG)
            assert(id < mEntityBySpatialId.size() &&
                   "SpatialIndexSystem: SpatialId32 out of range (free-list corrupted)");
            assert(mEntityBySpatialId[id] == core::ecs::NullEntity &&
                   "SpatialIndexSystem: SpatialId32 reused without release (mapping not cleared)");
#else
            if (id >= mEntityBySpatialId.size()) [[unlikely]] {
                LOG_PANIC(core::log::cat::ECS,
                          "SpatialIndexSystem: SpatialId32 out of range (free-list corrupted) "
                          "(id={})",
                          id);
            }
            if (mEntityBySpatialId[id] != core::ecs::NullEntity) [[unlikely]] {
                LOG_PANIC(core::log::cat::ECS,
                          "SpatialIndexSystem: SpatialId32 reused without release "
                          "(mapping not cleared) (id={})",
                          id);
            }
#endif
            return id;
        }

        if (mNextSpatialId >= mEntityBySpatialId.size()) {
            return 0;
        }

        const core::spatial::EntityId32 id = mNextSpatialId;
        ++mNextSpatialId;
        return id;
    }

    void SpatialIndexSystem::releaseSpatialId(const core::spatial::EntityId32 id) noexcept {
        if (id == 0) {
            return;
        }

#if !defined(NDEBUG)
        assert(id < mEntityBySpatialId.size() &&
               "SpatialIndexSystem: SpatialId32 out of range on release");
        assert(mEntityBySpatialId[id] != core::ecs::NullEntity &&
               "SpatialIndexSystem: release of unmapped SpatialId32 "
               "(double-free or contract violation)");
#else
        if (id >= mEntityBySpatialId.size()) [[unlikely]] {
            LOG_PANIC(core::log::cat::ECS,
                      "SpatialIndexSystem: SpatialId32 out of range on release (id={})", id);
        }
        if (mEntityBySpatialId[id] == core::ecs::NullEntity) [[unlikely]] {
            LOG_PANIC(core::log::cat::ECS,
                      "SpatialIndexSystem: release of unmapped SpatialId32 "
                      "(double-free or contract violation) (id={})",
                      id);
        }
#endif

        mEntityBySpatialId[id] = core::ecs::NullEntity;
        mFreeSpatialIds.push_back(id);
    }

    void SpatialIndexSystem::setMapping(const core::spatial::EntityId32 id,
                                        const Entity entity) noexcept {
        assert(id != 0 &&
               "SpatialIndexSystem: SpatialId32 must be non-zero");
        assert(id < mEntityBySpatialId.size() &&
               "SpatialIndexSystem: SpatialId32 out of range");
        assert(entity != core::ecs::NullEntity &&
               "SpatialIndexSystem: mapping NullEntity forbidden");
        mEntityBySpatialId[id] = entity;
    }

} // namespace core::ecs