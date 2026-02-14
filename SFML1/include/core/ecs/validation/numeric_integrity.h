// ================================================================================================
// File: core/ecs/validation/numeric_integrity.h
// Purpose: Validate-on-write numeric invariants for ECS components (Transform, Sprite).
//          Called ONLY at write boundaries (spawn, mutation).
//          Read hot-path (render, spatial query) trusts data (trust-on-read).
// Used by: PlayerInitSystem, MovementSystem
// Related headers: transform_component.h, sprite_component.h
// ================================================================================================
#pragma once

#include <cassert>
#include <cmath>
#include <string_view>

#include <SFML/System/Vector2.hpp>

#include "core/ecs/components/sprite_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/log/log_macros.h"

namespace core::ecs::validation {

    // --------------------------------------------------------------------------------------------
    // Чистые предикаты (без side effects). Можно использовать в условиях, не вызывая PANIC.
    // --------------------------------------------------------------------------------------------

    [[nodiscard]] inline bool isFiniteVec2(const sf::Vector2f v) noexcept {
        return std::isfinite(v.x) && std::isfinite(v.y);
    }

    [[nodiscard]] inline bool isFiniteFloat(const float v) noexcept {
        return std::isfinite(v);
    }

    [[nodiscard]] inline bool isTransformFinite(const TransformComponent& tr) noexcept {
        return isFiniteVec2(tr.position) && isFiniteFloat(tr.rotationDegrees);
    }

    [[nodiscard]] inline bool isSpriteFinite(const SpriteComponent& sp) noexcept {
        return isFiniteVec2(sp.scale) && isFiniteVec2(sp.origin) && isFiniteFloat(sp.zOrder);
    }

    // --------------------------------------------------------------------------------------------
    // Fail-fast assert/PANIC обёртки для write boundaries.
    // Scope: строго finite-check. Доменная валидация (диапазоны, scale==0) — не здесь.
    // --------------------------------------------------------------------------------------------

    /// Validate-on-write: все float-поля TransformComponent конечны (не NaN, не Inf).
    /// Вызывать после мутации transform на write boundary.
    inline void assertTransformFinite(const TransformComponent& tr,
                                      [[maybe_unused]] const std::string_view where) noexcept {
        const bool finite = isTransformFinite(tr);
#if !defined(NDEBUG)
        assert(finite && "numeric_integrity: TransformComponent содержит NaN/Inf");
#else
        if (!finite) [[unlikely]] {
            LOG_PANIC(core::log::cat::ECS,
                      "[{}] numeric_integrity: TransformComponent non-finite "
                      "(pos=({},{}) rot={})",
                      where, tr.position.x, tr.position.y, tr.rotationDegrees);
        }
#endif
    }

    /// Validate-on-write: все float-поля SpriteComponent конечны (не NaN, не Inf).
    /// Вызывать после мутации sprite на write boundary.
    inline void assertSpriteFinite(const SpriteComponent& sp,
                                   [[maybe_unused]] const std::string_view where) noexcept {
        const bool finite = isSpriteFinite(sp);
#if !defined(NDEBUG)
        assert(finite && "numeric_integrity: SpriteComponent содержит NaN/Inf");
#else
        if (!finite) [[unlikely]] {
            LOG_PANIC(core::log::cat::ECS,
                      "[{}] numeric_integrity: SpriteComponent non-finite "
                      "(scale=({},{}) origin=({},{}) z={})",
                      where, sp.scale.x, sp.scale.y,
                      sp.origin.x, sp.origin.y, sp.zOrder);
        }
#endif
    }

} // namespace core::ecs::validation