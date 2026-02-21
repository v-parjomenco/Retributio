# ================================================================================================
# File: SFML1/CMakeLists.txt
#
# Authoritative source for all build flags and project structure.
# Ground truth: SFML1.vcxproj (Debug / DebugTests / Profile / Release configurations).
#
# Requires CMake 4.2.3+. No backwards-compatibility shims — this project targets
# the latest toolchain exclusively.
#
# Configurations:
#   Debug       - /Od /ZI /RTC1 /JMC, /MDd, Console subsystem
#                 Defines: SFML_STATIC _DEBUG _CONSOLE _UNICODE UNICODE
#   DebugTests  - [Future CMake config] Same as Debug + SFML1_TESTS.
#                 sfml1_core_tests carries SFML1_TESTS unconditionally; adding the
#                 DebugTests CMake config later requires only CMakePresets.json update
#                 and adding $<CONFIG:DebugTests> to the relevant generator expressions here.
#   Profile     - /O2 /Ob2 /Ot /GL + /LTCG, /MD, Windows subsystem
#                 Defines: SFML_STATIC NDEBUG SFML1_PROFILE _UNICODE UNICODE
#                 INVARIANT: must NOT define _DEBUG.
#                 INVARIANT: full release-like optimization — /O2 /Ob2 /Ot /GL.
#                 Only difference from Release: SFML1_PROFILE defined, dev overlay visible.
#                 Compile-time enforcement in main_skyguard.cpp via #error.
#   Release     - /O2 /GL + /LTCG, /MD, Windows subsystem
#                 Defines: SFML_STATIC NDEBUG _UNICODE UNICODE
#
# Dev library policy:
#   sfml1_skyguard_dev is a dev OVERLAY library — NOT a debug-only library.
#   It provides runtime visibility (stats, spatial metrics, debug overlays) in:
#     Debug   — correctness + visibility, no optimization
#     Profile — full optimization, visibility preserved for profiling sessions
#     Release — completely absent (empty TUs, excluded via generator expression)
#   The overlay guards itself: #if !defined(NDEBUG) || defined(SFML1_PROFILE)
#   Profile is a RELEASE-LIKE build: /O2 /Ob2 /Ot /GL, /MD, NDEBUG defined, no RTC.
#   sfml1_skyguard_dev does NOT alter Profile's optimization profile in any way.
#
#   sfml1_core_tests — linked in Debug only (correctness assertions, not perf)
#   sfml1_spatial_harness executable — built in Debug + Profile
#   sfml1_render_stress executable   — built in Debug + Profile
#
# Phase 1 changes (see ROADMAP.md § Phase 1):
#   + sfml1_stress_common: new STATIC target, owns stress_runtime_stamp.cpp +
#     stress_chunk_content_provider.cpp (physically in src/game/skyguard/dev/).
#     Breaks the LNK2001 time bomb:
#
#     BEFORE (broken):
#       sfml1_skyguard_dev ──uses──► StressRuntimeStamp
#                                            ↑
#       sfml1_spatial_harness_lib ──owns─────┘
#                 └──► sfml1_skyguard (FrameOrchestrator)
#       sfml1_game (Profile) linked sfml1_skyguard_dev but NOT sfml1_spatial_harness_lib
#       → LNK2001 the moment main_skyguard.cpp gets real code
#
#     AFTER (correct, acyclic):
#       sfml1_stress_common → sfml1_core only (no skyguard/dev deps)
#       sfml1_skyguard_dev  → sfml1_stress_common (PRIVATE — impl detail, not public API)
#       sfml1_spatial_harness_lib → sfml1_stress_common (PRIVATE)
#       sfml1_game → sfml1_stress_common (explicit, same genex as sfml1_skyguard_dev)
#
#     Why sfml1_game needs explicit sfml1_stress_common:
#       STATIC lib PRIVATE deps are NOT propagated to consumers — by CMake design.
#       Linker resolves symbols only when building the final EXE/DLL.
#       sfml1_game (EXE) must declare every lib it needs at link time.
#       Making sfml1_stress_common PUBLIC in sfml1_skyguard_dev would be wrong:
#       it would leak stress headers into every consumer's include path.
#
#   + sfml1_skyguard_dev: links sfml1_stress_common PRIVATE
#   + sfml1_spatial_harness_lib: source list trimmed, links sfml1_stress_common PRIVATE
#   + sfml1_game: links sfml1_stress_common in Debug+DebugTests+Profile
#   + stress_small / stress_large custom targets — one-click from MSVC or CLI
#   + COMPILE_WARNING_AS_ERROR target property (CMake 4.x) used where applicable
# ================================================================================================

# ------------------------------------------------------------------------------------------------
# Language standards
# C17  — required for third_party/xxhash/upstream/xxhash.c
# C++20 — required for all engine and game code
# Extensions disabled: strict conformance, no MSVC/GCC non-standard extensions.
# ------------------------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)
set(CMAKE_C_STANDARD            17)
set(CMAKE_C_STANDARD_REQUIRED   ON)
set(CMAKE_C_EXTENSIONS          OFF)

# ------------------------------------------------------------------------------------------------
# MSVC Runtime Library
# Debug / DebugTests : /MDd  (MultiThreadedDebugDLL)
# Release / Profile  : /MD   (MultiThreadedDLL)
#
# SFML is linked statically (SFML_STATIC), so no SFML .dll is needed at runtime.
# CRT is dynamic (/MD, not /MT): fully static CRT causes multiple heap instances
# when the process loads any DLL, leading to heap corruption on cross-module alloc/free.
# Source of truth: vcxproj <RuntimeLibrary> per config.
# ------------------------------------------------------------------------------------------------
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY
        "MultiThreaded$<$<OR:$<CONFIG:Debug>,$<CONFIG:DebugTests>>:Debug>DLL")
endif()

# ------------------------------------------------------------------------------------------------
# Profile configuration: CMake has no built-in knowledge of this custom config name.
# All CMAKE-injected default flag variables must be cleared; flags are controlled
# exclusively via sfml1_build_options INTERFACE target below.
#
# Without this reset, CMake silently injects Release defaults (/O2 /Ob2 /DNDEBUG)
# into Profile at configure time, duplicating our explicit flags and being invisible
# to the developer unless they grep the generated .rsp files.
# ------------------------------------------------------------------------------------------------
if(MSVC)
    foreach(_v IN ITEMS
            CMAKE_C_FLAGS_PROFILE
            CMAKE_CXX_FLAGS_PROFILE
            CMAKE_EXE_LINKER_FLAGS_PROFILE
            CMAKE_SHARED_LINKER_FLAGS_PROFILE
            CMAKE_STATIC_LINKER_FLAGS_PROFILE)
        set(${_v} "" CACHE STRING "" FORCE)
    endforeach()
endif()

# ------------------------------------------------------------------------------------------------
# MSVC debug information format (CMake 3.25+ / CMake 4.x)
#
# CMake injects /Zi into CMAKE_CXX_FLAGS_DEBUG by default. Our targets need /ZI
# (Edit-and-Continue) for Debug, and /Zi (ProgramDatabase) for Release/Profile.
# Having both on the command line causes D9025 "overriding /Zi with /ZI" on every TU.
#
# The correct solution (CMake 3.25+) is CMAKE_MSVC_DEBUG_INFORMATION_FORMAT.
# When this variable is set BEFORE any target is created, CMake:
#   1. Completely stops injecting debug-info flags into CMAKE_CXX_FLAGS_* / CMAKE_C_FLAGS_*
#   2. Injects the correct per-config flag via the target property mechanism instead
#
# Values: EditAndContinue → /ZI   ProgramDatabase → /Zi   (empty) → no flag
#
# This replaces the previous regex-strip hack on CMAKE_CXX_FLAGS_DEBUG, which was
# fragile and broke on cache invalidation. The property mechanism is authoritative.
#
# Must be set before add_library / add_executable — CMake reads it at target creation time.
# ------------------------------------------------------------------------------------------------
if(MSVC)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
        "$<$<OR:$<CONFIG:Debug>,$<CONFIG:DebugTests>>:EditAndContinue>$<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:ProgramDatabase>"
    )
endif()

# ------------------------------------------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------------------------------------------
# IMPORTANT: CMAKE_MAP_IMPORTED_CONFIG_<CONFIG> must be set as a regular (non-cache)
# variable in the calling scope BEFORE find_package. CMake reads this variable at the
# moment it creates each imported target and uses it to initialize the per-target
# MAP_IMPORTED_CONFIG_<CONFIG> property. A cache-only value (from CMakePresets.json)
# does NOT trigger this initialization — only a regular variable in scope does.
#
# Setting it here covers all find_package calls below, including the transitive ones
# invoked by SFMLConfig.cmake (FindVorbis, FindFLAC, etc.). Those modules ship only
# DEBUG and RELEASE variants; "Profile" is unknown to them. Without this mapping,
# CMake's Ninja Multi-Config generator fails at generate time with:
#   "IMPORTED_LOCATION not set for imported target Vorbis::vorbis configuration Profile"
set(CMAKE_MAP_IMPORTED_CONFIG_PROFILE "Release")

message(STATUS "SFML1: CMAKE_MAP_IMPORTED_CONFIG_PROFILE = ${CMAKE_MAP_IMPORTED_CONFIG_PROFILE}")

find_package(SFML CONFIG REQUIRED COMPONENTS Network Graphics Window Audio System Main)

# ------------------------------------------------------------------------------------------------
# Profile configuration fix for config-less transitive targets (Vorbis, FLAC).
#
# These targets are created by vcpkg's FindVorbis / FindFLAC modules as config-less
# imported targets: no IMPORTED_CONFIGURATIONS, a single IMPORTED_LOCATION (no suffix).
# CMake's Ninja Multi-Config generator validates per-config locations for every config
# listed in CMAKE_CONFIGURATION_TYPES. It requires IMPORTED_LOCATION_<CONFIG> to exist
# for each config. IMPORTED_LOCATION (no suffix) does NOT satisfy this check for "Profile".
# MAP_IMPORTED_CONFIG_PROFILE is used at link time but is NOT consulted during generator
# validation — that check is purely suffix-based.
#
# Fix: copy IMPORTED_LOCATION → IMPORTED_LOCATION_PROFILE on each affected target.
# Diagnostic confirmed: IMPORTED_LOCATION = .../debug/lib/vorbis.lib (config-less).
# MAP_IMPORTED_CONFIG_PROFILE = Release is already set, so link time uses the Release lib.
# ------------------------------------------------------------------------------------------------
foreach(_t IN ITEMS Ogg::ogg Vorbis::vorbis Vorbis::vorbisfile Vorbis::vorbisenc FLAC::FLAC)
    if(NOT TARGET ${_t})
        continue()
    endif()

    get_target_property(_loc_debug ${_t} IMPORTED_LOCATION)
    if(NOT _loc_debug)
        continue()
    endif()

    # vcpkg layout: debug libs live under   .../debug/lib/<n>.lib
    #               release libs live under  .../lib/<n>.lib
    string(REPLACE "/debug/lib/" "/lib/" _loc_release "${_loc_debug}")
    if(NOT EXISTS "${_loc_release}")
        message(WARNING
            "SFML1: release lib not found at '${_loc_release}', "
            "falling back to debug lib for ${_t}")
        set(_loc_release "${_loc_debug}")
    endif()

    # Transition the target from config-less to config-aware mode.
    # Config-less imported targets use a single IMPORTED_LOCATION for ALL configs,
    # but the Ninja Multi-Config generator validates each config independently.
    # Setting IMPORTED_CONFIGURATIONS explicitly makes IMPORTED_LOCATION_<CONFIG>
    # the authoritative per-config property, which the generator accepts.
    set_target_properties(${_t} PROPERTIES
        IMPORTED_CONFIGURATIONS   "DEBUG;RELEASE;PROFILE"
        IMPORTED_LOCATION_DEBUG   "${_loc_debug}"
        IMPORTED_LOCATION_RELEASE "${_loc_release}"
        IMPORTED_LOCATION_PROFILE "${_loc_release}"
    )

    message(STATUS "SFML1: patched ${_t}")
    message(STATUS "  IMPORTED_LOCATION_DEBUG   = ${_loc_debug}")
    message(STATUS "  IMPORTED_LOCATION_RELEASE = ${_loc_release}")
    message(STATUS "  IMPORTED_LOCATION_PROFILE = ${_loc_release}")
endforeach()
unset(_t)
unset(_loc_debug)
unset(_loc_release)

# ================================================================================================
# sfml1_build_options  (INTERFACE library — compile flags for ALL project targets)
# sfml1::build_options (ALIAS         — use this name in target_link_libraries)
#
# Single authoritative source of compiler flags.
# Every flag maps 1:1 to a vcxproj setting. No flag is added without a vcxproj counterpart.
# All per-config variation is handled here via generator expressions.
# ================================================================================================
add_library(sfml1_build_options INTERFACE)
add_library(sfml1::build_options ALIAS sfml1_build_options)

target_compile_features(sfml1_build_options INTERFACE cxx_std_20)

# ----------------------------------------------------------------
# Preprocessor definitions
# Mirrors vcxproj <PreprocessorDefinitions> per config exactly.
# ----------------------------------------------------------------
target_compile_definitions(sfml1_build_options INTERFACE

    # ── All configurations ───────────────────────────────────────
    SFML_STATIC        # SFML linked as static library (no .dll dependency)
    _UNICODE           # Windows: use Unicode API (wchar_t)
    UNICODE            # Windows: use Unicode API (C++ side)

    # ── Debug / DebugTests ───────────────────────────────────────
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:DebugTests>>:_DEBUG>
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:DebugTests>>:_CONSOLE>

    # ── Release / Profile  (no _DEBUG, no _CONSOLE) ──────────────
    # NOTE: CMake also injects NDEBUG for Release via CMAKE_CXX_FLAGS_RELEASE.
    # We define it here explicitly so Profile gets it through the same path.
    $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:NDEBUG>

    # ── Profile only ─────────────────────────────────────────────
    # SFML1_PROFILE activates dev overlay at compile time.
    # Profile remains release-optimized: /O2 /Ob2 /Ot /GL, NDEBUG, no RTC.
    $<$<CONFIG:Profile>:SFML1_PROFILE>
)

if(MSVC)
    # ----------------------------------------------------------------
    # Universal compiler flags — ALL configurations, ALL languages
    # Each flag maps to an explicit vcxproj ClCompile setting.
    # ----------------------------------------------------------------
    target_compile_options(sfml1_build_options INTERFACE
        /utf-8               # Source and execution charset: UTF-8 (AdditionalOptions)
        /permissive-         # Strict conformance (ConformanceMode=true)
        /wd4710              # Disable: 'function' not inlined (DisableSpecificWarnings)
        /wd4711              # Disable: function selected for inline expansion
        /sdl                 # SDL security checks (SDLCheck=true); superset of /GS
        /GS                  # Buffer security checks (default, explicit for clarity)
        /fp:precise          # Precise floating-point (FloatingPointModel=Precise)
        /FC                  # Full path in diagnostics (UseFullPaths=true)
        /diagnostics:column  # Report column numbers (DiagnosticsFormat=Column)
        /Zc:wchar_t          # wchar_t as native type
        /Zc:inline           # Remove unreferenced COMDAT
        # External-header warning suppression.
        # CRITICAL: without these, /Wall + /WX causes thousands of errors in
        # SFML / EnTT / nlohmann / STL headers, making the project unbuildable.
        /external:anglebrackets  # Mark <angle-bracket> includes as external
        /external:W0             # Zero warnings from external headers
        /analyze:external-       # No code analysis on external headers
    )

    # ── C++-only flags (all configs) ────────────────────────────
    target_compile_options(sfml1_build_options INTERFACE
        $<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>   # Correct __cplusplus macro value
        $<$<COMPILE_LANGUAGE:CXX>:/Zc:preprocessor>  # Conformant preprocessor
        $<$<COMPILE_LANGUAGE:CXX>:/EHsc>             # C++ exception handling model
        $<$<COMPILE_LANGUAGE:CXX>:/Wall>             # Enable all warnings
    )

    # ── C-only flags (all configs) ──────────────────────────────
    target_compile_options(sfml1_build_options INTERFACE
        $<$<COMPILE_LANGUAGE:C>:/Wall>
    )

    # ----------------------------------------------------------------
    # Debug / DebugTests — no optimization, edit-and-continue, runtime checks
    # Source: vcxproj Debug|x64 ClCompile
    # ----------------------------------------------------------------
    target_compile_options(sfml1_build_options INTERFACE
        $<$<OR:$<CONFIG:Debug>,$<CONFIG:DebugTests>>:/Od>    # Disable optimization
        # /ZI (EditAndContinue PDB) is injected by CMake via CMAKE_MSVC_DEBUG_INFORMATION_FORMAT.
        # Do NOT duplicate it here — that causes D9025 "overriding /Zi with /ZI" on every TU.
        $<$<OR:$<CONFIG:Debug>,$<CONFIG:DebugTests>>:/RTC1>  # Runtime checks (stack, uninit)
        $<$<OR:$<CONFIG:Debug>,$<CONFIG:DebugTests>>:/JMC>   # Just My Code debugging
    )

    # ----------------------------------------------------------------
    # Release / Profile — shared optimization baseline
    # Source: vcxproj Release|x64 and Profile|x64 ClCompile
    #
    # Profile is NOT a copy of Release — it is a separate explicit config.
    # Both have identical optimization flags; Profile additionally defines
    # SFML1_PROFILE which enables the dev overlay at compile time.
    # ----------------------------------------------------------------
    target_compile_options(sfml1_build_options INTERFACE
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:/O2>  # Maximize speed
        # /Zi (ProgramDatabase) is injected by CMake via CMAKE_MSVC_DEBUG_INFORMATION_FORMAT.
        # Do NOT duplicate it here — that causes D9025.
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:/GL>  # Whole-program optimization
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:/Gy>  # Function-level linking
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:/Oi>  # Enable intrinsic functions
    )

    # ----------------------------------------------------------------
    # Profile only — additional speed flags beyond Release baseline
    # Source: vcxproj Profile|x64 ClCompile
    #   /Ob2 → InlineFunctionExpansion=AnySuitable
    #   /Ot  → FavorSizeOrSpeed=Speed
    # ----------------------------------------------------------------
    target_compile_options(sfml1_build_options INTERFACE
        $<$<CONFIG:Profile>:/Ob2>   # Inline: any suitable function
        $<$<CONFIG:Profile>:/Ot>    # Favor fast code over small code
    )

endif() # MSVC

# ------------------------------------------------------------------------------------------------
# COMPILE_WARNING_AS_ERROR — CMake 4.x target property.
# Replaces manual /WX flag injection. Applied to sfml1_build_options so every
# consumer inherits it. xxhash source file overrides this via per-source COMPILE_OPTIONS
# (/WX-) which takes precedence over the target-level property on MSVC.
# ------------------------------------------------------------------------------------------------
set_target_properties(sfml1_build_options PROPERTIES
    COMPILE_WARNING_AS_ERROR ON
)

# ================================================================================================
# Per-source overrides
# Declared before targets so properties are visible at target configuration time.
# ================================================================================================
set(_pch_cpp  "${CMAKE_CURRENT_SOURCE_DIR}/src/pch.cpp")
set(_xxhash_c "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/xxhash/upstream/xxhash.c")

# pch.cpp: CMake manages PCH via target_precompile_headers; pch.cpp is kept in
# sources for IDE file-tree visibility but must NOT participate in CMake's PCH
# mechanism (otherwise CMake tries to apply /Yu to a file compiled with /Yc).
set_source_files_properties("${_pch_cpp}" PROPERTIES
    SKIP_PRECOMPILE_HEADERS ON
)

# xxhash.c: C source from third-party. Must not use PCH (C vs C++ PCH incompatibility).
# Per-source settings mirror vcxproj <ClCompile Include="...xxhash.c"> item overrides:
#   PrecompiledHeader       = NotUsing  → SKIP_PRECOMPILE_HEADERS ON
#   WarningLevel            = Level4   → /W4  (overrides /Wall from build_options)
#   TreatWarningAsError     = false    → /WX- (overrides COMPILE_WARNING_AS_ERROR ON)
#   DisableSpecificWarnings = 4710;4711;4820;5045
#     4820 — padding bytes added after struct member (common in C)
#     5045 — Spectre mitigation informational (not actionable in third-party code)
set_source_files_properties("${_xxhash_c}" PROPERTIES
    SKIP_PRECOMPILE_HEADERS ON
)
if(MSVC)
    # APPEND so these come after sfml1_build_options flags on the command line.
    # MSVC applies the last /Wn and last /WX[+/-] seen, so ordering matters.
    set_property(SOURCE "${_xxhash_c}" APPEND PROPERTY COMPILE_OPTIONS
        /W4 /WX- /wd4710 /wd4711 /wd4820 /wd5045
    )
endif()

# ================================================================================================
# sfml1_core  —  Engine-layer static library (ECS, spatial, resources, logging, UI utils)
# ================================================================================================
add_library(sfml1_core STATIC)

target_link_libraries(sfml1_core
    PUBLIC
        sfml1::build_options
        SFML::System
        SFML::Window
        SFML::Graphics
)

target_include_directories(sfml1_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party
)

target_sources(sfml1_core PRIVATE
    "${_xxhash_c}"
    "${_pch_cpp}"

    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/config/loader/debug_overlay_loader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/config/loader/detail/non_critical_config_report.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/config/loader/engine_settings_loader.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/ecs/stable_id_service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/ecs/systems/debug_overlay_system.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/ecs/systems/movement_system.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/ecs/systems/render_system.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/ecs/systems/spatial_index_system.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/ecs/world.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/log/logging.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/log/log_level.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/resources/keys/stable_key.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/resources/registry/resource_registry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/resources/registry/string_pool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/resources/resource_manager.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/spatial/spatial_index.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/spatial/spatial_index_v2.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/time/time_service.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/ui/anchor_policy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/ui/ids/ui_id_utils.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/utils/file_loader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/utils/json/json_document.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/utils/json/json_parsers.cpp
)

# PCH: each target compiles its own PCH from include/pch.h.
# REUSE_FROM is fundamentally incompatible with Ninja Multi-Config + MSVC:
# it requires all consumers to use the identical /Fd path, but CMake generates
# per-target /Fd paths, causing C2859. Per-target PCH is the correct solution —
# pch.h rarely changes, the extra compilation cost is negligible.
target_precompile_headers(sfml1_core PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/pch.h>"
)

# ================================================================================================
# sfml1_core_tests  —  Engine self-tests
#
# Linked in Debug (and future DebugTests config) only — not in Profile or Release.
# SFML1_TESTS is defined unconditionally on this target because sfml1_core_tests
# is exclusively a test artifact. When DebugTests CMake config is added, extend
# the generator expression in sfml1_game's target_link_libraries accordingly.
# ================================================================================================
add_library(sfml1_core_tests STATIC)

target_link_libraries(sfml1_core_tests
    PUBLIC
        sfml1::build_options
        sfml1_core
)

# SFML1_TESTS enables self-test code paths in the source files of this target.
# Matches vcxproj DebugTests|x64 <PreprocessorDefinitions> which adds SFML1_TESTS.
target_compile_definitions(sfml1_core_tests PRIVATE SFML1_TESTS)

target_sources(sfml1_core_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/resources/registry/resource_registry_self_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/resources/registry/string_pool_self_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/resources/resource_manager_self_test.cpp
)

target_precompile_headers(sfml1_core_tests PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/pch.h>"
)

# C4189: local variable initialized but not referenced.
# Test files capture intermediate results for debugger inspection even when
# assert() expands to nothing under NDEBUG. Intentional test idiom — the variable
# exists to be breakpoint-inspectable, not to produce side effects.
# Suppressed per-target to avoid polluting other targets.
if(MSVC)
    target_compile_options(sfml1_core_tests PRIVATE /wd4189)
endif()

# ================================================================================================
# sfml1_skyguard  —  SkyGuard game runtime (pure game layer, no dev tooling)
# ================================================================================================
add_library(sfml1_skyguard STATIC)

target_link_libraries(sfml1_skyguard
    PUBLIC
        sfml1::build_options
        sfml1_core
        SFML::Graphics
        SFML::Window
        SFML::Audio
        SFML::Network
)

target_include_directories(sfml1_skyguard
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(sfml1_skyguard PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/skyguard/bootstrap/scene_bootstrap.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/skyguard/config/loader/app_config_loader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/skyguard/config/loader/config_loader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/skyguard/config/loader/spatial_v2_config_builder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/skyguard/config/loader/user_settings_loader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/skyguard/config/loader/view_config_loader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/skyguard/config/loader/window_config_loader.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/skyguard/ecs/systems/player_init_system.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/skyguard/game.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/skyguard/orchestration/frame_orchestrator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/skyguard/platform/user_paths.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/skyguard/presentation/background_renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/skyguard/presentation/view_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/skyguard/presentation/window_mode_manager.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/skyguard/utils/debug_format.cpp
)

target_precompile_headers(sfml1_skyguard PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/pch.h>"
)

# ================================================================================================
# [PHASE 1] sfml1_stress_common  —  Shared stress infrastructure
#
# Owns the two stress source files previously embedded in sfml1_spatial_harness_lib.
# Extracting them breaks the LNK2001 time bomb described in the file header.
#
# Dependency contract (enforced by PRIVATE linkage everywhere):
#   sfml1_stress_common → sfml1_core
#   sfml1_stress_common → NOTHING from sfml1_skyguard or sfml1_skyguard_dev
#
# Rationale: stress_common is consumed by BOTH sfml1_skyguard_dev (game tool layer)
# and sfml1_spatial_harness_lib (standalone tool) — it must not depend on either.
# Depending on sfml1_skyguard here would create a cycle:
#   sfml1_skyguard_dev → sfml1_stress_common → sfml1_skyguard → sfml1_skyguard_dev  [CYCLE]
#
# Physical file locations are UNCHANGED — CMake ownership is what defines the boundary.
# ================================================================================================
add_library(sfml1_stress_common STATIC)

target_link_libraries(sfml1_stress_common
    PUBLIC
        sfml1::build_options
        sfml1_core
)

target_sources(sfml1_stress_common PRIVATE
    # Files are physically in src/game/skyguard/dev/ — location is historical artifact.
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/skyguard/dev/stress_runtime_stamp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/skyguard/dev/stress_chunk_content_provider.cpp
)

target_precompile_headers(sfml1_stress_common PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/pch.h>"
)

# ================================================================================================
# sfml1_skyguard_dev  —  Developer overlay (active in Debug AND Profile)
#
# This is NOT a "debug-only" library. It is a DEVELOPER VISIBILITY library that
# provides runtime statistics, spatial metrics, and debug overlays in two configs:
#
#   Debug   — no optimization (/Od), full runtime checks (/RTC1), overlay visible
#   Profile — full optimization (/O2 /Ob2 /Ot /GL), NO runtime checks, overlay visible
#             Profile is release-like in every performance-relevant way. The overlay
#             exists here to provide profiling data during performance sessions.
#   Release — overlay completely absent (empty TUs after preprocessor; excluded by genex)
#
# Source guard in overlay_extras.cpp and background_math_self_test.cpp:
#   #if !defined(NDEBUG) || defined(SFML1_PROFILE)
#   Debug  : NDEBUG not defined → compiles
#   Profile: NDEBUG defined BUT SFML1_PROFILE defined → compiles
#   Release: NDEBUG defined, SFML1_PROFILE not defined → empty TU
#
# [PHASE 1] sfml1_stress_common linked PRIVATE:
#   PRIVATE because stress types (StressRuntimeStamp etc.) are implementation details
#   of overlay_extras.cpp — they do NOT appear in sfml1_skyguard_dev's public headers.
#   Consumers of sfml1_skyguard_dev must NOT see or link sfml1_stress_common implicitly.
#   sfml1_game therefore links sfml1_stress_common explicitly (see sfml1_game below).
# ================================================================================================
add_library(sfml1_skyguard_dev STATIC)

target_link_libraries(sfml1_skyguard_dev
    PUBLIC
        sfml1::build_options
        sfml1_skyguard
    PRIVATE
        sfml1_stress_common  # [PHASE 1] impl detail: StressRuntimeStamp, StressChunkContentProvider
)

target_sources(sfml1_skyguard_dev PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/skyguard/dev/overlay_extras.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/skyguard/presentation/background_math_self_test.cpp
)

target_precompile_headers(sfml1_skyguard_dev PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/pch.h>"
)

# ================================================================================================
# sfml1_spatial_harness_lib  —  Spatial index correctness + timing harness
#
# spatial_index_harness.cpp guards: #if !defined(NDEBUG) || defined(SFML1_PROFILE)
# Debug:   correctness tests (V1 vs V2 result matching, determinism checks)
# Profile: performance benchmarks (query/update timing, p95/p99 latencies)
#
# Requires sfml1_skyguard because spatial_index_harness.cpp uses FrameOrchestrator
# (implementation in frame_orchestrator.cpp, owned by sfml1_skyguard).
#
# [PHASE 1] stress_runtime_stamp.cpp and stress_chunk_content_provider.cpp removed
# from this target — ownership transferred to sfml1_stress_common.
# sfml1_stress_common is linked PRIVATE: its types are implementation details of
# spatial_index_harness.cpp and do not appear in sfml1_spatial_harness_lib headers.
# ================================================================================================
add_library(sfml1_spatial_harness_lib STATIC)

target_link_libraries(sfml1_spatial_harness_lib
    PUBLIC
        sfml1::build_options
        sfml1_core
        sfml1_skyguard
    PRIVATE
        sfml1_stress_common  # [PHASE 1] ownership transferred from here to sfml1_stress_common
)

target_sources(sfml1_spatial_harness_lib PRIVATE
    # [PHASE 1] stress_runtime_stamp.cpp + stress_chunk_content_provider.cpp removed.
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/skyguard/dev/spatial_index_harness.cpp
)

target_precompile_headers(sfml1_spatial_harness_lib PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/pch.h>"
)

# ================================================================================================
# sfml1_render_stress_lib  —  Render stress test (INTERFACE; entry point stub only for now)
# ================================================================================================
add_library(sfml1_render_stress_lib INTERFACE)

target_link_libraries(sfml1_render_stress_lib
    INTERFACE
        sfml1::build_options
        sfml1_skyguard
)

# ================================================================================================
# Executables
# ================================================================================================

# ------------------------------------------------------------------------------------------------
# sfml1_game  —  SkyGuard game entry point
# ------------------------------------------------------------------------------------------------
add_executable(sfml1_game
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/skyguard/entry/main_skyguard.cpp
)

# WIN32_EXECUTABLE: tells CMake this is a Windows-subsystem app.
# Without this, CMake unconditionally injects /subsystem:console, conflicting with
# our per-config /SUBSYSTEM:CONSOLE|WINDOWS below. We own the subsystem flag entirely.
set_target_properties(sfml1_game PROPERTIES WIN32_EXECUTABLE TRUE)

target_link_libraries(sfml1_game PRIVATE
    sfml1_skyguard

    # Dev overlay: Debug + Profile only. Release: excluded (empty TUs, saves link time).
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:DebugTests>,$<CONFIG:Profile>>:sfml1_skyguard_dev>

    # [PHASE 1] sfml1_stress_common: must be listed explicitly here.
    # sfml1_skyguard_dev links sfml1_stress_common PRIVATE — PRIVATE deps of a STATIC
    # lib are NOT propagated to consumers (STATIC libs do not link; only EXE/DLL do).
    # sfml1_game (EXE) is where actual linking happens, so it must name every lib
    # whose symbols it needs. Same activation set as sfml1_skyguard_dev: no stress
    # code reaches Release binary.
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:DebugTests>,$<CONFIG:Profile>>:sfml1_stress_common>

    # Self-tests: Debug + DebugTests only (correctness, not perf measurement)
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:DebugTests>>:sfml1_core_tests>
)

target_precompile_headers(sfml1_game PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/pch.h>"
)

# ------------------------------------------------------------------------------------------------
# sfml1_spatial_harness  —  Standalone spatial index benchmark / correctness harness
# Console tool: reads config from env vars, prints timing to stdout, exits.
# ------------------------------------------------------------------------------------------------
add_executable(sfml1_spatial_harness
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/skyguard/entry/main_spatial_harness.cpp
)

target_link_libraries(sfml1_spatial_harness PRIVATE
    sfml1_spatial_harness_lib
)

target_precompile_headers(sfml1_spatial_harness PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/pch.h>"
)

# ------------------------------------------------------------------------------------------------
# sfml1_render_stress  —  Render stress test (entry point stub; no real code yet)
# ------------------------------------------------------------------------------------------------
add_executable(sfml1_render_stress
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/skyguard/entry/main_render_stress.cpp
)
set_target_properties(sfml1_render_stress PROPERTIES WIN32_EXECUTABLE TRUE)

target_link_libraries(sfml1_render_stress PRIVATE
    sfml1_render_stress_lib
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:DebugTests>,$<CONFIG:Profile>>:sfml1_skyguard_dev>
)

target_precompile_headers(sfml1_render_stress PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/pch.h>"
)

# ================================================================================================
# SFML::Main — Windows entry point wrapper
#
# Provides WinMain → delegates to int main(). Required for /SUBSYSTEM:WINDOWS builds
# (Release + Profile) where the OS calls WinMain, not main() directly.
# Linked in all configs per vcxproj (sfml-main[-d].lib in all <AdditionalDependencies>).
# sfml1_spatial_harness excluded: pure console tool, no windowed entry point needed.
# ================================================================================================
target_link_libraries(sfml1_game          PRIVATE SFML::Main)
target_link_libraries(sfml1_render_stress PRIVATE SFML::Main)

# ================================================================================================
# Linker options (MSVC only)
# Source of truth: vcxproj <Link> section per config.
# ================================================================================================
if(MSVC)

    # ----------------------------------------------------------------
    # sfml1_game
    # Debug/DebugTests : /SUBSYSTEM:CONSOLE  (vcxproj <SubSystem>Console)
    # Release/Profile  : /SUBSYSTEM:WINDOWS  (vcxproj <SubSystem>Windows)
    # Release/Profile  : /LTCG               (vcxproj <LinkTimeCodeGeneration>)
    # Profile          : /INCREMENTAL:NO     (vcxproj <LinkIncremental>false; implied by /LTCG)
    # ----------------------------------------------------------------
    target_link_options(sfml1_game PRIVATE
        $<$<OR:$<CONFIG:Debug>,$<CONFIG:DebugTests>>:/SUBSYSTEM:CONSOLE>
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:/SUBSYSTEM:WINDOWS>
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:/LTCG>
        $<$<CONFIG:Profile>:/INCREMENTAL:NO>
    )

    # ----------------------------------------------------------------
    # sfml1_render_stress — same subsystem logic as sfml1_game (has a render window)
    # ----------------------------------------------------------------
    target_link_options(sfml1_render_stress PRIVATE
        $<$<OR:$<CONFIG:Debug>,$<CONFIG:DebugTests>>:/SUBSYSTEM:CONSOLE>
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:/SUBSYSTEM:WINDOWS>
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:/LTCG>
        $<$<CONFIG:Profile>:/INCREMENTAL:NO>
    )

    # ----------------------------------------------------------------
    # sfml1_spatial_harness — always Console (CLI benchmark, no GUI window)
    # ----------------------------------------------------------------
    target_link_options(sfml1_spatial_harness PRIVATE
        /SUBSYSTEM:CONSOLE
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:/LTCG>
        $<$<CONFIG:Profile>:/INCREMENTAL:NO>
    )

endif() # MSVC

# ================================================================================================
# [PHASE 1] Stress preset custom targets
#
# One-click stress runs from MSVC Solution Explorer (right-click target → Build)
# or from the command line:
#   cmake --build --preset win-profile --target stress_small
#   cmake --build --preset win-profile --target stress_large
#
# Architecture:
#   .env files (tools/presets/) — source of truth for stress configuration (12-factor)
#   cmake/run_with_env.cmake    — portable env-file runner (no shell required)
#   add_custom_target           — convenience wrapper for MSVC and CLI; thin layer only
#
# DEPENDS ensures sfml1_spatial_harness is built before the custom target runs.
# USES_TERMINAL routes child process stdout/stderr directly to MSVC Output window.
#
# Preset path: CMAKE_SOURCE_DIR is the Solution root (parent of SFML1/).
# Directory layout assumed:
#   <SolutionDir>/
#   ├── SFML1/          ← CMAKE_CURRENT_SOURCE_DIR (this file)
#   ├── tools/
#   │   └── presets/
#   │       ├── active_small.env
#   │       └── active_large.env
#   └── CMakeLists.txt  ← CMAKE_SOURCE_DIR
# ================================================================================================
set(_stress_presets_dir  "${CMAKE_SOURCE_DIR}/tools/presets")
set(_run_with_env_script "${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_with_env.cmake")

add_custom_target(stress_small
    COMMAND ${CMAKE_COMMAND}
        -DENV_FILE=${_stress_presets_dir}/active_small.env
        -DEXE=$<TARGET_FILE:sfml1_spatial_harness>
        -P "${_run_with_env_script}"
    DEPENDS sfml1_spatial_harness
    COMMENT "[stress_small] sfml1_spatial_harness + active_small.env (~1K entities, smoke)"
    USES_TERMINAL
)

add_custom_target(stress_large
    COMMAND ${CMAKE_COMMAND}
        -DENV_FILE=${_stress_presets_dir}/active_large.env
        -DEXE=$<TARGET_FILE:sfml1_spatial_harness>
        -P "${_run_with_env_script}"
    DEPENDS sfml1_spatial_harness
    COMMENT "[stress_large] sfml1_spatial_harness + active_large.env (~1M+ entities, soak)"
    USES_TERMINAL
)

unset(_stress_presets_dir)
unset(_run_with_env_script)