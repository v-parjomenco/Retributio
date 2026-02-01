#include "pch.h"

#include "core/ecs/systems/spatial_index_system.h"

#include <cassert>
#include <cmath>

#include "core/ecs/components/spatial_dirty_tag.h"
#include "core/ecs/components/spatial_id_component.h"
#include "core/ecs/components/sprite_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/render/sprite_bounds.h"
#include "core/ecs/world.h"
#include "core/log/log_macros.h"
#include "core/utils/math_constants.h"

namespace {
    [[nodiscard]] core::spatial::Aabb2 computeEntityAabb(
        const core::ecs::TransformComponent& tr, const core::ecs::SpriteComponent& sp) noexcept {
        const float rotationDegrees = tr.rotationDegrees;
        if (rotationDegrees == 0.f) {
            return core::ecs::render::computeSpriteAabbNoRotation(tr.position, sp.origin,
                                                                   sp.scale, sp.textureRect);
        }
        const float radians = rotationDegrees * core::utils::kDegToRad;
        const float cachedSin = std::sin(radians);
        const float cachedCos = std::cos(radians);
        return core::ecs::render::computeSpriteAabbRotated(tr.position, sp.origin, sp.scale,
                                                            sp.textureRect, cachedSin, cachedCos);
    }
} // namespace

namespace core::ecs {

    SpatialIndexSystem::SpatialIndexSystem(const SpatialIndexSystemConfig& config) noexcept
        : mIndex() {
        assert(config.maxEntityId > 0 && "SpatialIndexSystem: maxEntityId must be > 0");
        assert(config.index.maxEntityId > 0 && "SpatialIndexSystem: index.maxEntityId must be > 0");
        assert(config.index.cellSizeWorld > 0 && "SpatialIndexSystem: cellSizeWorld must be > 0");
        assert(config.index.chunkSizeWorld > 0 && "SpatialIndexSystem: chunkSizeWorld must be > 0");
        assert(config.index.maxEntityId == config.maxEntityId &&
               "SpatialIndexSystem: index.maxEntityId must match maxEntityId");

        mIndex.init(config.index, config.storage);
        const auto origin = config.storage.origin;
        for (std::int32_t y = origin.y; y < origin.y + config.storage.height; ++y) {
            for (std::int32_t x = origin.x; x < origin.x + config.storage.width; ++x) {
                const bool loaded =
                    mIndex.setChunkState({x, y}, core::spatial::ResidencyState::Loaded);
                if (!loaded) {
                    LOG_PANIC(core::log::cat::ECS,
                              "SpatialIndexSystem: failed to set chunk loaded ({}, {})", x, y);
                }
            }
        }

        mEntityBySpatialId.clear();
        mEntityBySpatialId.resize(static_cast<std::size_t>(config.maxEntityId) + 1u,
                                  core::ecs::NullEntity);
        mFreeSpatialIds.clear();
        mFreeSpatialIds.reserve(static_cast<std::size_t>(config.maxEntityId) / 16u);
        mNextSpatialId = 1u;
        mDeterminismEnabled = config.determinismEnabled;
    }

    void SpatialIndexSystem::update(World& world, float) {
        ensureDestroyConnection(world);

        auto& registry = world.registry();

        if (mDeterminismEnabled && !mStableIdsPrepared) {
            world.stableIds().enable();
            world.stableIds().prewarm(mEntityBySpatialId.size());
            mStableIdsPrepared = true;
        }

        auto newView = registry.view<TransformComponent, SpriteComponent>(
            entt::exclude<SpatialIdComponent>);


        for (auto [entity, transform, sprite] : newView.each()) {
            assert(core::ecs::render::hasExplicitRect(sprite.textureRect) &&
                   "SpatialIndexSystem: SpriteComponent.textureRect must be explicit");

            const core::spatial::Aabb2 aabb = computeEntityAabb(transform, sprite);

            const core::spatial::EntityId32 id = allocateSpatialId();
            if (id == 0) {
                LOG_PANIC(core::log::cat::ECS,
                          "SpatialIndexSystem: SpatialId32 pool exhausted (maxEntityId={})",
                          mEntityBySpatialId.size() - 1u);
            }

            const core::spatial::WriteResult result = mIndex.registerEntity(id, aabb);
            if (result == core::spatial::WriteResult::Rejected) {
                releaseSpatialId(id);
                LOG_PANIC(core::log::cat::ECS,
                          "SpatialIndexSystem: registerEntity rejected (id={})", id);
            }
            if (result == core::spatial::WriteResult::PartialTruncated) {
                releaseSpatialId(id);
                LOG_PANIC(core::log::cat::ECS,
                          "SpatialIndexSystem: registerEntity truncated (id={})", id);
            }

            // entity гарантированно не имел SpatialIdComponent (exclude<>),
            // поэтому emplace достаточно.
            registry.emplace<SpatialIdComponent>(entity, SpatialIdComponent{id, aabb});
            setMapping(id, entity);
        }

        auto dirtyView = registry.view<SpatialIdComponent,
                                       SpatialDirtyTag,
                                       TransformComponent,
                                       SpriteComponent>();

        bool hadDirty = false;
        if (mDeterminismEnabled) {
            if (mDirtyScratch.capacity() < dirtyView.size_hint()) {
                mDirtyScratch.reserve(dirtyView.size_hint());
            }
            mDirtyScratch.clear();

            for (const Entity entity : dirtyView) {
                mDirtyScratch.push_back(entity);
            }

            // Детерминированное присвоение StableID должно происходить в фиксированном порядке:
            //  1) сортируем по runtime entity id (toUint) как стабильному tie-break 
            //     в рамках процесса;
            //  2) в этом порядке вызываем ensureAssigned() ровно один раз на сущность;
            //  3) после этого сортируем по StableID без write-path в comparator.
            std::sort(mDirtyScratch.begin(), mDirtyScratch.end(),
                      [](const Entity a, const Entity b) noexcept {
                          return core::ecs::toUint(a) < core::ecs::toUint(b);
                      });

            auto& stableIds = world.stableIds();
            for (const Entity e : mDirtyScratch) {
                (void) stableIds.ensureAssigned(e);
            }

            std::sort(mDirtyScratch.begin(), mDirtyScratch.end(),
                      [&stableIds](const Entity a, const Entity b) {
                          const auto aIdOpt = stableIds.tryGet(a);
                          const auto bIdOpt = stableIds.tryGet(b);
                          if (!aIdOpt.has_value() || !bIdOpt.has_value()) {
                              LOG_PANIC(core::log::cat::ECS,
                                        "SpatialIndexSystem: missing StableID during deterministic sort");
                          }
                          return *aIdOpt < *bIdOpt;
                      });

            for (const Entity entity : mDirtyScratch) {
                auto& handleComp = registry.get<SpatialIdComponent>(entity);
                const auto& transform = registry.get<TransformComponent>(entity);
                const auto& sprite = registry.get<SpriteComponent>(entity);

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
                if (result == core::spatial::WriteResult::PartialTruncated) {
                    LOG_PANIC(core::log::cat::ECS,
                              "SpatialIndexSystem: updateEntity truncated (id={})", handleComp.id);
                }
                handleComp.lastAabb = newAabb;
            }
        } else {
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
                if (result == core::spatial::WriteResult::PartialTruncated) {
                    LOG_PANIC(core::log::cat::ECS,
                              "SpatialIndexSystem: updateEntity truncated (id={})", handleComp.id);
                }
                handleComp.lastAabb = newAabb;
            }
        }

        // ВАЖНО: не удаляем SpatialDirtyTag внутри итерации view (риск инвалидации).
        // storage SpatialDirtyTag содержит только "грязные" сущности, поэтому clear() = O(dirty),
        // без аллокаций и без лишних проходов по "чистым" сущностям.
        if (hadDirty) {
            registry.clear<SpatialDirtyTag>();
        }
    }

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
               "SpatialIndexSystem: double-free SpatialId32");
#else
        if (id >= mEntityBySpatialId.size()) [[unlikely]] {
            LOG_PANIC(core::log::cat::ECS,
                      "SpatialIndexSystem: SpatialId32 out of range on release (id={})", id);
        }
        if (mEntityBySpatialId[id] == core::ecs::NullEntity) [[unlikely]] {
            LOG_PANIC(core::log::cat::ECS, "SpatialIndexSystem: double-free SpatialId32 (id={})",
                      id);
        }
#endif

        mEntityBySpatialId[id] = core::ecs::NullEntity;
        mFreeSpatialIds.push_back(id);
    }

    void SpatialIndexSystem::setMapping(const core::spatial::EntityId32 id,
                                        const Entity entity) noexcept {
        assert(id < mEntityBySpatialId.size() && "SpatialIndexSystem: SpatialId32 out of range");
        mEntityBySpatialId[id] = entity;
    }

} // namespace core::ecs