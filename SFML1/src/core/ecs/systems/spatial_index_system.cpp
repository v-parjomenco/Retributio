#include "pch.h"

#include "core/ecs/systems/spatial_index_system.h"

#include <cassert>
#include <cmath>

#include "core/ecs/components/spatial_dirty_tag.h"
#include "core/ecs/components/spatial_handle_component.h"
#include "core/ecs/components/sprite_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/render/sprite_bounds.h"
#include "core/ecs/world.h"
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

    SpatialIndexSystem::SpatialIndexSystem(float cellSize) noexcept
        : mIndex(cellSize) {
    }

    void SpatialIndexSystem::update(World& world, float) {
        ensureDestroyConnection(world);

        auto& registry = world.registry();

        auto newView = registry.view<TransformComponent, SpriteComponent>(
            entt::exclude<SpatialHandleComponent>);

        for (const Entity entity : newView) {
            const auto& transform = newView.get<TransformComponent>(entity);
            const auto& sprite = newView.get<SpriteComponent>(entity);

#if !defined(NDEBUG)
            assert(core::ecs::render::hasExplicitRect(sprite.textureRect) &&
                   "SpatialIndexSystem: SpriteComponent.textureRect must be explicit");
#endif

            const core::spatial::Aabb2 aabb = computeEntityAabb(transform, sprite);

            const std::uint32_t handle = mIndex.registerEntity(entity, aabb);

            // entity гарантированно не имел SpatialHandleComponent (exclude<>),
            // поэтому emplace достаточно.
            registry.emplace<SpatialHandleComponent>(entity, SpatialHandleComponent{handle, aabb});
        }

        auto dirtyView = registry.view<SpatialHandleComponent,
                                       SpatialDirtyTag,
                                       TransformComponent,
                                       SpriteComponent>();

        bool hadDirty = false;
        for (const Entity entity : dirtyView) {
            hadDirty = true;
            auto& handleComp = dirtyView.get<SpatialHandleComponent>(entity);
            const auto& transform = dirtyView.get<TransformComponent>(entity);
            const auto& sprite = dirtyView.get<SpriteComponent>(entity);

#if !defined(NDEBUG)
            assert(core::ecs::render::hasExplicitRect(sprite.textureRect) &&
                   "SpatialIndexSystem: SpriteComponent.textureRect must be explicit");
#endif

            const core::spatial::Aabb2 newAabb = computeEntityAabb(transform, sprite);

            mIndex.update(handleComp.handle, handleComp.lastAabb, newAabb);
            handleComp.lastAabb = newAabb;
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
        mOnDestroyConnection = registry.on_destroy<SpatialHandleComponent>().connect<
            &SpatialIndexSystem::onHandleDestroyed>(*this);
        mConnected = true;
    }

    void SpatialIndexSystem::onHandleDestroyed(entt::registry& registry, Entity entity) noexcept {
        if (const auto* handle = registry.try_get<SpatialHandleComponent>(entity)) {
            if (handle->handle != 0) {
                mIndex.unregister(handle->handle, handle->lastAabb);
            }
        }
    }

} // namespace core::ecs