// ================================================================================================
// File: third_party/entt/entt_registry.hpp
// Purpose: Full EnTT includes (registry, view, group) with MSVC warning suppression
// Used by: Implementation files (.cpp) working with EnTT directly
// Related headers: entt_fwd.hpp (forward-only version for headers)
// ================================================================================================
#pragma once

#if defined(_MSC_VER)
// Отключение предупреждений только для внутренних файлов EnTT:
//  - C4464: относительный путь поиска включаемых файлов содержит '..';
//  - C4868: порядок вычисления в списке инициализаторов.
    #pragma warning(push)
    #pragma warning(disable : 4464 4868)
#endif

#include <entt/entity/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <entt/entity/view.hpp>
#include <entt/entity/group.hpp>

#if defined(_MSC_VER)
    #pragma warning(pop)
#endif