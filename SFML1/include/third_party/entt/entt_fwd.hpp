// ================================================================================================
// File: third_party/entt/entt_fwd.hpp
// Purpose: Forward declarations from EnTT with MSVC warning suppression
// Used by: Headers that need entt::entity type but not full registry definition
// Related headers: entt_registry.hpp (for full EnTT includes)
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

#if defined(_MSC_VER)
	#pragma warning(pop)
#endif