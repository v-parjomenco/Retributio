// ================================================================================================
// File: core/compiler/platform/windows.h
// Purpose: Platform-specific compiler switches for Windows builds
// Used by: pch.h, any translation unit that needs Windows macros
// ================================================================================================
#pragma once

#if defined(_WIN32)

// Reduce <Windows.h> header bloat if it ever gets included anywhere.
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif

// Avoid global min/max macros that break std::min/std::max.
#if !defined(NOMINMAX)
#define NOMINMAX
#endif

#endif // _WIN32