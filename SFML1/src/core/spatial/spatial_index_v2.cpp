#include "pch.h"

#include "core/spatial/spatial_index_v2.h"

#include "adapters/entt/entt_registry.hpp"

namespace core::spatial {

#if defined(SFML1_TITAN)
    static_assert(sizeof(entt::entity) == 4,
                  "Titan requires 32-bit entt::entity backend");
#endif

} // namespace core::spatial