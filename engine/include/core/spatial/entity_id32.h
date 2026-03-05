// ================================================================================================
// File: core/spatial/entity_id32.h
// Purpose: Lightweight EntityId32 type alias for SpatialIndexV2 integration.
// ================================================================================================
#pragma once

#include <cstdint>

namespace core::spatial {

    // EntityId32: НЕ entt::entity. Внешний 32-bit ID вызывающей стороны.
    // Индексирует marks/overflow; требует prewarm(maxEntityId) перед write/query.
    using EntityId32 = std::uint32_t;

} // namespace core::spatial