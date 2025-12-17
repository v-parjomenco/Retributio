// ================================================================================================
// File: core/utils/json/json_common.h
// Purpose: Common JSON type alias for engine code (core::utils::json::json).
// Used by: core/utils/json/*, config loaders, resource loaders.
// Notes:
//  - This header is intentionally light: it pulls only forward declarations.
//  - Include third_party/json/json_silent.hpp in .cpp (or headers with inline/template JSON logic).
// ================================================================================================
#pragma once

#include "third_party/json/json_fwd.hpp"

namespace core::utils::json {

    using json = nlohmann::json;

} // namespace core::utils::json
