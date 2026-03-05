// ================================================================================================
// File: adapters/json/json_silent.hpp
// Purpose: Wrapper around <nlohmann/json.hpp> with warning suppression for third-party code.
// Notes:
//  - Suppresses noisy diagnostics coming from nlohmann-json internals only.
// ================================================================================================
#pragma once

#if defined(_MSC_VER)
    #pragma warning(push)
    // Все эти отключения применяются только к коду nlohmann-json,
    // чтобы не засорять билд ворнингами в чужой библиотеке.
    #pragma warning(disable : 26819) // unannotated fallthrough / SAL issues in switch
    #pragma warning(disable : 26495) // uninitialized member (analysis)
    #pragma warning(disable : 28251) // incomplete _Param_ annotation
    #pragma warning(disable : 4996)  // deprecated std::copy, std::iterator etc.
    #pragma warning(disable : 4100)  // unreferenced formal parameter
    #pragma warning(disable : 4505)  // unreferenced local function removed
    #pragma warning(disable : 4702)  // unreachable code
    #pragma warning(disable : 4458)  // declaration hides class member
#endif

#include <nlohmann/json.hpp>

#if defined(_MSC_VER)
    #pragma warning(pop)
#endif