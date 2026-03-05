// ================================================================================================
// File: adapters/xxhash/xxhash.h
// Purpose: Include xxHash (xxhash.h) with a stable path and optional warning suppression.
// Used by: core/resources/keys/stable_key.h (StableKey64 computation at load/tool time)
// Notes:
//  - Pure include wrapper - zero runtime cost.
// ================================================================================================
#pragma once

#include <xxhash/upstream/xxhash.h>