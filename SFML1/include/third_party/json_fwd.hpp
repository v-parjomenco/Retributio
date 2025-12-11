// ================================================================================================
// File: third_party/json_fwd.hpp
// Purpose: Thin wrapper around nlohmann/json_fwd.hpp for forward declarations.
// Use this in headers that only need nlohmann::json as a type (no methods), to avoid pulling the
//  full json.hpp everywhere.
// Notes:
//  - Does NOT pull the full nlohmann/json.hpp heavy header.
//  - Intended exclusively for headers to reduce compile-time and dependencies.
// ================================================================================================
#pragma once

#include <nlohmann/json_fwd.hpp>