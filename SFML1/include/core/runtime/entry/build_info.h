// ================================================================================================
// File: core/runtime/entry/build_info.h
// Purpose: Compile-time build metadata (version, build type, date, git commit hash).
// Used by: Entry point mains (main_skyguard, main_spatial_harness, main_render_stress).
// Related: core/log/log_macros.h
// Notes:
//  - Header-only by design: __DATE__, __TIME__, GIT_COMMIT_HASH are compile-time macros
//    unique per translation unit. A .cpp would capture the build_info.cpp compile time
//    instead of each main's own compile time — incorrect behaviour.
//  - GIT_COMMIT_HASH is injected by CMake via target_compile_definitions on EXE targets.
//    The #ifndef fallback handles builds without Git or outside CMake.
//  - Build consistency #error guards are centralised here; removed from individual mains.
// ================================================================================================
#pragma once

// GIT_COMMIT_HASH fallback ОБЯЗАН стоять до определения BuildInfo, т.к. используется
// в инициализаторе поля. CMake-define имеет приоритет: #ifndef не перезапишет его.
#ifndef GIT_COMMIT_HASH
    #define GIT_COMMIT_HASH "<unknown>"
#endif

// Гарантии корректности конфигурации сборки — единая точка проверки для всех entry points.
#if defined(_DEBUG) && defined(NDEBUG)
    #error "Invalid build: both _DEBUG and NDEBUG are defined."
#endif
#if defined(SFML1_PROFILE) && defined(_DEBUG)
    #error "Invalid build: Profile must not define _DEBUG (use NDEBUG + SFML1_PROFILE)."
#endif

#include "core/log/log_macros.h"

namespace core::runtime::entry {

    struct BuildInfo {
        const char* version   = "0.1.0-dev";
        const char* buildDate = __DATE__ " " __TIME__;
        const char* buildType =
#if defined(SFML1_PROFILE)
            "Profile";
#elif defined(NDEBUG)
            "Release";
#else
            "Debug";
#endif
        const char* gitCommit = GIT_COMMIT_HASH;
    };

    /// Логирует метаданные сборки через LOG_INFO(Engine).
    /// Требует активного LoggingLifetime до вызова.
    inline void logBuildInfo() {
        const BuildInfo info{};
        LOG_INFO(core::log::cat::Engine,
                 "Build info: version={}, type={}, built at {}, commit={}",
                 info.version, info.buildType, info.buildDate, info.gitCommit);
    }

} // namespace core::runtime::entry
