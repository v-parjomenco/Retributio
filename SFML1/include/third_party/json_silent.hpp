#pragma once

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable: 26819) // switch fallthrough
    #pragma warning(disable: 26495) // uninitialized member
    #pragma warning(disable: 28251) // incomplete annotation
    #pragma warning(disable: 4996)  // deprecated std::copy etc.
    #pragma warning(disable: 4100)  // unreferenced formal parameter
    #pragma warning(disable: 4505)  // unreferenced local function removed
    #pragma warning(disable: 4702)  // unreachable code
    #pragma warning(disable: 4458)  // hides class member
#endif

#include "json.hpp"

#ifdef _MSC_VER
    #pragma warning(pop)
#endif