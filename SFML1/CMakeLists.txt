# =================================================================================================
# File: SFML1/CMakeLists.txt
#
# Authoritative source for all build flags and project structure.
# Ground truth: SFML1.vcxproj (Debug / Profile / Release configurations).
#
# Requires CMake 4.2.3+. No backwards-compatibility shims — this project targets
# the latest toolchain exclusively.
#
# Configurations:
#   Debug       - /Od /ZI /RTC1 /JMC, /MDd, Console subsystem
#                 Defines: SFML_STATIC _DEBUG _CONSOLE _UNICODE UNICODE
#   Profile     - /O2 /Ob2 /Ot /GL + /LTCG, /MD, Windows subsystem
#                 Defines: SFML_STATIC NDEBUG SFML1_PROFILE _UNICODE UNICODE
#                 INVARIANT: must NOT define _DEBUG.
#                 INVARIANT: full release-like optimization — /O2 /Ob2 /Ot /GL.
#                 Only difference from Release: SFML1_PROFILE defined, dev overlay visible.
#                 Compile-time enforcement in build_info.h via #error.
#   Release     - /O2 /GL + /LTCG, /MD, Windows subsystem
#                 Defines: SFML_STATIC NDEBUG _UNICODE UNICODE
#
# Target graph (dependency direction: A -> B means A depends on B):
#
#   sfml1_core                — engine artifact: ECS, spatial, resources, logging.
#                               Declares core::log::detail::panic_sink but does NOT define it.
#                               Declares core::resources::detail::hashCanonicalKey but does NOT
#                               define it (StableKey seam).
#   sfml1_stable_key_xxhash   — STATIC; defines StableKey seam:
#                               core::resources::detail::hashCanonicalKey via xxHash3_64.
#                               Linked by sfml1_runtime (production).
#                               Tests provide their own seam TU and must NOT link this target.
#   sfml1_sfml_audio          — STATIC; SFML audio backend for ISoundResource.
#                               Links SFML::Audio PUBLIC (self-sufficient).
#                               Private headers isolated in include_private/.
#   sfml1_panic_sink_default  — STATIC; defines panic_sink → MessageBox + std::exit.
#   sfml1_runtime             — INTERFACE; composes sfml1_core + sfml1_panic_sink_default
#                               + sfml1_stable_key_xxhash (production seam set).
#                               All game targets link sfml1_runtime, not sfml1_core directly.
#                               Tests link sfml1_core + sfml1_core_test_support instead and
#                               provide StableKey seam TU explicitly.
#   sfml1_runtime_entry       — STATIC; engine-layer entry policy (logging lifetime,
#                               terminate handler, single-instance guard, working directory,
#                               user dialogs, build info). Consumed PRIVATELY by EXE targets only.
#   sfml1_skyguard            -> sfml1_runtime, sfml1_sfml_audio(SFML1_ENABLE_AUDIO),
#                                SFML::Graphics, SFML::Window, SFML::Network
#   sfml1_stress_common       -> sfml1_runtime  (no sfml1_skyguard dep — avoids cycle)
#   sfml1_skyguard_dev        -> sfml1_skyguard, sfml1_stress_common(PRIVATE)
#   sfml1_spatial_harness_lib -> sfml1_runtime, sfml1_skyguard, sfml1_stress_common(PRIVATE)
#   sfml1_render_stress_lib   -> sfml1_skyguard  (INTERFACE stub)
#   sfml1_game                -> sfml1_skyguard, sfml1_skyguard_dev(Debug+Profile),
#                                sfml1_stress_common(Debug+Profile), sfml1_runtime_entry
#   sfml1_spatial_harness     -> sfml1_spatial_harness_lib, sfml1_runtime_entry
#   sfml1_render_stress       -> sfml1_render_stress_lib, sfml1_runtime_entry
#
# Dev overlay policy (sfml1_skyguard_dev):
#   Active in Debug (no optimization) and Profile (full /O2 optimization).
#   Absent in Release: excluded by generator expression.
#   Source guard: #if !defined(NDEBUG) || defined(SFML1_PROFILE)
#
# sfml1_stress_common ownership:
#   Owns stress_runtime_stamp.cpp + stress_chunk_content_provider.cpp.
#   Consumed PRIVATELY by sfml1_skyguard_dev and sfml1_spatial_harness_lib.
#   Must not depend on sfml1_skyguard: both consumers already depend on it,
#   adding sfml1_skyguard here would create a dependency cycle.
#   Physical file location (src/game/skyguard/dev/) is a historical artifact;
#   CMake target membership defines ownership, not filesystem path.
#
#   Why sfml1_game links sfml1_stress_common explicitly:
#     STATIC lib PRIVATE deps are NOT propagated to EXE consumers by CMake design.
#     sfml1_game (EXE) must name every lib it needs at link time.
# =================================================================================================

# -------------------------------------------------------------------------------------------------
# Language standards
# C17  — required for third_party/xxhash/upstream/xxhash.c
# C++20 — required for all engine and game code
# Extensions disabled: strict conformance, no MSVC/GCC non-standard extensions.
# -------------------------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)
set(CMAKE_C_STANDARD            17)
set(CMAKE_C_STANDARD_REQUIRED   ON)
set(CMAKE_C_EXTENSIONS          OFF)

option(SFML1_ENABLE_AUDIO "Enable SFML audio module" ON)

# -------------------------------------------------------------------------------------------------
# MSVC Runtime Library
# Debug   : /MDd  (MultiThreadedDebugDLL)
# Release / Profile  : /MD   (MultiThreadedDLL)
#
# SFML is linked statically (SFML_STATIC), so no SFML .dll is needed at runtime.
# CRT is dynamic (/MD, not /MT): fully static CRT causes multiple heap instances
# when the process loads any DLL, leading to heap corruption on cross-module alloc/free.
# Source of truth: vcxproj <RuntimeLibrary> per config.
# -------------------------------------------------------------------------------------------------
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY
        "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# -------------------------------------------------------------------------------------------------
# Profile configuration: CMake has no built-in knowledge of this custom config name.
# All CMAKE-injected default flag variables must be cleared; flags are controlled
# exclusively via sfml1_build_options INTERFACE target below.
# Without this reset CMake silently duplicates Release defaults into Profile.
# -------------------------------------------------------------------------------------------------
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

# -------------------------------------------------------------------------------------------------
# Dependencies
# -------------------------------------------------------------------------------------------------
message(STATUS "SFML1: CMAKE_MAP_IMPORTED_CONFIG_PROFILE = ${CMAKE_MAP_IMPORTED_CONFIG_PROFILE}")

if(SFML1_ENABLE_AUDIO)
    find_package(SFML CONFIG REQUIRED COMPONENTS
        Network Graphics Window Audio System Main)
else()
    find_package(SFML CONFIG REQUIRED COMPONENTS
        Network Graphics Window System Main)
endif()

# -------------------------------------------------------------------------------------------------
# Config-less imported targets fix (Vorbis, FLAC)
#
# vcpkg's FindVorbis/FindFLAC create targets with IMPORTED_LOCATION (no config suffix)
# instead of per-config IMPORTED_LOCATION_<CONFIG>. Ninja Multi-Config validates
# per-config locations for every config in CMAKE_CONFIGURATION_TYPES and rejects
# config-less targets for Profile. MAP_IMPORTED_CONFIG_PROFILE handles link time but
# NOT generator validation. Fix: promote each target to config-aware mode explicitly.
# -------------------------------------------------------------------------------------------------
foreach(_t IN ITEMS Ogg::ogg Vorbis::vorbis Vorbis::vorbisfile Vorbis::vorbisenc FLAC::FLAC)
    if(NOT TARGET ${_t})
        continue()
    endif()

    get_target_property(_loc_debug ${_t} IMPORTED_LOCATION)
    if(NOT _loc_debug)
        continue()
    endif()

    # vcpkg layout: debug libs under .../debug/lib/, release libs under .../lib/
    string(REPLACE "/debug/lib/" "/lib/" _loc_release "${_loc_debug}")
    if(NOT EXISTS "${_loc_release}")
        message(WARNING
            "SFML1: release lib not found at '${_loc_release}', "
            "falling back to debug lib for ${_t}")
        set(_loc_release "${_loc_debug}")
    endif()

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

# -------------------------------------------------------------------------------------------------
# sfml1_build_options  (INTERFACE library — compile flags for ALL project targets)
# sfml1::build_options (ALIAS         — use this name in target_link_libraries)
#
# Single authoritative source of compiler flags.
# Every flag maps 1:1 to a vcxproj setting. No flag is added without a vcxproj counterpart.
# All per-config variation is handled here via generator expressions.
# -------------------------------------------------------------------------------------------------
add_library(sfml1_build_options INTERFACE)
add_library(sfml1::build_options ALIAS sfml1_build_options)

target_compile_features(sfml1_build_options INTERFACE cxx_std_20)

# -------------------------------------------------------------------------------------------------
# Preprocessor definitions
# Mirrors vcxproj <PreprocessorDefinitions> per config exactly.
# -------------------------------------------------------------------------------------------------
target_compile_definitions(sfml1_build_options INTERFACE

    # All configurations
    SFML_STATIC        # SFML linked as static library (no .dll dependency)
    _UNICODE           # Windows: use Unicode API (wchar_t)
    UNICODE            # Windows: use Unicode API (C++ side)

    # Debug
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Debug>:_CONSOLE>

    # Release / Profile — defined here so Profile gets NDEBUG through the same path as Release.
    $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:NDEBUG>

    # Profile only — activates dev overlay; does not affect optimization profile.
    $<$<CONFIG:Profile>:SFML1_PROFILE>
)

if(MSVC)
    # ---------------------------------------------------------------------------------------------
    # Universal compiler flags — ALL configurations, ALL languages
    # Each flag maps to an explicit vcxproj ClCompile setting.
    # ---------------------------------------------------------------------------------------------
    target_compile_options(sfml1_build_options INTERFACE
        /utf-8               # Source and execution charset: UTF-8
        /permissive-         # Strict conformance
        /wd4710              # Disable: 'function' not inlined
        /wd4711              # Disable: function selected for inline expansion
        /sdl                 # SDL security checks (superset of /GS)
        /GS                  # Buffer security checks
        /fp:precise          # Precise floating-point
        /FC                  # Full path in diagnostics
        /diagnostics:column  # Report column numbers
        /Zc:wchar_t          # wchar_t as native type
        /Zc:inline           # Remove unreferenced COMDAT
        # Without these, /Wall + /WX produces thousands of errors in external headers.
        /external:anglebrackets  # Mark <angle-bracket> includes as external
        /external:W0             # Zero warnings from external headers
        /analyze:external-       # No code analysis on external headers
    )

    # C++-only flags (all configs)
    target_compile_options(sfml1_build_options INTERFACE
        $<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>   # Correct __cplusplus macro value
        $<$<COMPILE_LANGUAGE:CXX>:/Zc:preprocessor>  # Conformant preprocessor
        $<$<COMPILE_LANGUAGE:CXX>:/EHsc>             # C++ exception handling model
        $<$<COMPILE_LANGUAGE:CXX>:/Wall>             # All warnings
    )

    # C-only flags (all configs)
    target_compile_options(sfml1_build_options INTERFACE
        $<$<COMPILE_LANGUAGE:C>:/Wall>
    )

    # Debug
    target_compile_options(sfml1_build_options INTERFACE
        $<$<CONFIG:Debug>:/Od>    # Disable optimization
        # /ZI injected via CMAKE_MSVC_DEBUG_INFORMATION_FORMAT — do not duplicate here.
        $<$<CONFIG:Debug>:/RTC1>  # Runtime checks (stack, uninit)
        $<$<CONFIG:Debug>:/JMC>   # Just My Code debugging
    )

    # Release / Profile — shared optimization baseline
    target_compile_options(sfml1_build_options INTERFACE
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:/O2>  # Maximize speed
        # /Zi injected via CMAKE_MSVC_DEBUG_INFORMATION_FORMAT — do not duplicate here.
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:/GL>  # Whole-program optimization
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:/Gy>  # Function-level linking
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:/Oi>  # Enable intrinsic functions
    )

    # Profile only — additional speed flags beyond Release baseline
    target_compile_options(sfml1_build_options INTERFACE
        $<$<CONFIG:Profile>:/Ob2>   # Inline: any suitable function
        $<$<CONFIG:Profile>:/Ot>    # Favor fast code over small code
    )

endif() # MSVC

# COMPILE_WARNING_AS_ERROR (CMake 4.x) — inherited by all consumers via sfml1_build_options.
# xxhash overrides this per-source with /WX- (see per-source block below).
set_target_properties(sfml1_build_options PROPERTIES
    COMPILE_WARNING_AS_ERROR ON
)

# -------------------------------------------------------------------------------------------------
# Per-source overrides
# Declared before targets so properties are visible at target configuration time.
# -------------------------------------------------------------------------------------------------
set(_pch_cpp  "${CMAKE_CURRENT_SOURCE_DIR}/src/pch.cpp")
set(_xxhash_c "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/xxhash/upstream/xxhash.c")

# pch.cpp: kept in sources for IDE visibility only; must not participate in CMake's
# PCH mechanism (applying /Yu to a /Yc file causes a build error).
set_source_files_properties("${_pch_cpp}" PROPERTIES
    SKIP_PRECOMPILE_HEADERS ON
)

# xxhash.c: third-party C source; incompatible with the C++ PCH.
# /W4 /WX- override the project-wide /Wall + COMPILE_WARNING_AS_ERROR.
# /wd4820 (struct padding) and /wd5045 (Spectre) are expected in C third-party code.
set_source_files_properties("${_xxhash_c}" PROPERTIES
    SKIP_PRECOMPILE_HEADERS ON
)
if(MSVC)
    # APPEND: these flags follow sfml1_build_options flags on the command line.
    # MSVC applies the last /Wn and last /WX[+/-] seen, so ordering matters.
    set_property(SOURCE "${_xxhash_c}" APPEND PROPERTY COMPILE_OPTIONS
        /W4 /WX- /wd4710 /wd4711 /wd4820 /wd5045
    )
endif()

# -------------------------------------------------------------------------------------------------
# sfml1_core  —  Engine-layer static library (ECS, spatial, resources, logging, UI utils)
#
# core::log::detail::panic_sink is declared in logging.h but NOT defined in this target.
# The definition is provided at link time by sfml1_panic_sink_default (production) or
# sfml1_core_test_support (tests). sfml1_runtime below is the canonical production
# composition that satisfies this requirement for all game targets.
#
# StableKey seam:
# core::resources::detail::hashCanonicalKey is declared in stable_key.h but NOT defined here.
# The production definition is provided by sfml1_stable_key_xxhash (linked by sfml1_runtime).
# Tests provide their own seam TU and must NOT link sfml1_stable_key_xxhash.
# -------------------------------------------------------------------------------------------------
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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/log/detail/utf8_to_wide.cpp

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

# Per-target PCH: REUSE_FROM is incompatible with Ninja Multi-Config + MSVC (/Fd path conflict).
target_precompile_headers(sfml1_core PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/pch.h>"
)

# -------------------------------------------------------------------------------------------------
# sfml1_stable_key_xxhash — production link-time seam for StableKey64
#
# Defines core::resources::detail::hashCanonicalKey via xxHash3_64.
# The xxHash implementation is provided by third_party/xxhash/upstream/xxhash.c
# which is compiled into sfml1_core.
#
# IMPORTANT:
#  - Production targets must obtain this seam via sfml1_runtime.
#  - Test targets must provide their own seam TU and must NOT link this target.
# -------------------------------------------------------------------------------------------------
add_library(sfml1_stable_key_xxhash STATIC)

target_link_libraries(sfml1_stable_key_xxhash
    PUBLIC
        sfml1::build_options
    PRIVATE
        sfml1_core
)

target_sources(sfml1_stable_key_xxhash PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/resources/keys/stable_key_impl_xxhash.cpp
)

target_precompile_headers(sfml1_stable_key_xxhash PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/pch.h>"
)

# -------------------------------------------------------------------------------------------------
# sfml1_sfml_audio  —  SFML audio backend for core::resources::ISoundResource
#
# Self-sufficient static library: links SFML::Audio PUBLIC so consumers get it
# transitively without naming SFML::Audio themselves.
#
# Header isolation (include_private/):
#   SFML-specific implementation headers (sfml_sound_resource.h) live in include_private/
#   and are visible ONLY to this target. External targets cannot resolve them even though
#   sfml1_core exports include/ as PUBLIC — include_private/ is a disjoint tree.
#   Public API surface is a single registration header:
#     include/core/backends/sfml_audio/sfml_audio_loader.h
#
# sfml1_core linkage is PRIVATE: this target's .cpp files need core headers for compilation,
# but every consumer already obtains sfml1_core through sfml1_runtime. If a future consumer
# links sfml1_sfml_audio without sfml1_runtime, promote sfml1_core to PUBLIC here.
# -------------------------------------------------------------------------------------------------
if(SFML1_ENABLE_AUDIO)
    add_library(sfml1_sfml_audio STATIC)

    target_link_libraries(sfml1_sfml_audio
        PUBLIC
            SFML::Audio
        PRIVATE
            sfml1_core
    )

    # PRIVATE: only this target's TUs see include_private/.
    # Public headers (sfml_audio_loader.h) live in include/ and are reachable
    # via sfml1_core's PUBLIC include directory — no duplication needed here.
    target_include_directories(sfml1_sfml_audio
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include_private
    )

    target_sources(sfml1_sfml_audio PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/backends/sfml_audio/sfml_audio_loader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/backends/sfml_audio/sfml_sound_resource.cpp
    )

    target_precompile_headers(sfml1_sfml_audio PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/pch.h>"
    )
endif()

# -------------------------------------------------------------------------------------------------
# sfml1_panic_sink_default — production panic policy (MessageBox + std::exit)
#
# Provides the definition of core::log::detail::panic_sink required by sfml1_core.
# Linked PRIVATE to sfml1_core to access logging internals and utf8ToWide without
# leaking those as additional PUBLIC dependencies of this target.
# Win32: explicit user32 dependency for MessageBoxW.
# -------------------------------------------------------------------------------------------------
add_library(sfml1_panic_sink_default STATIC)

target_link_libraries(sfml1_panic_sink_default
    PUBLIC
        sfml1::build_options
    PRIVATE
        sfml1_core
)

if(WIN32)
    target_link_libraries(sfml1_panic_sink_default PRIVATE user32)
endif()

target_include_directories(sfml1_panic_sink_default
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(sfml1_panic_sink_default PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/log/panic_sink_default.cpp
)

target_precompile_headers(sfml1_panic_sink_default PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/pch.h>"
)

# -------------------------------------------------------------------------------------------------
# sfml1_runtime — production runtime: engine + panic policy + StableKey seam (no audio)
#
# INTERFACE composition point for all game targets. Links sfml1_core,
# sfml1_panic_sink_default, and sfml1_stable_key_xxhash so consumers get a complete,
# linkable product without naming seam libraries explicitly.
#
# Audio is NOT part of the minimal runtime — game targets that need audio
# link sfml1_sfml_audio explicitly (see sfml1_skyguard).
#
# Test targets link sfml1_core + sfml1_core_test_support directly,
# bypassing sfml1_panic_sink_default entirely and providing StableKey seam TU.
# -------------------------------------------------------------------------------------------------
add_library(sfml1_runtime INTERFACE)

target_link_libraries(sfml1_runtime
    INTERFACE
        sfml1_core
        sfml1_panic_sink_default
        sfml1_stable_key_xxhash
)

# -------------------------------------------------------------------------------------------------
# sfml1_runtime_entry — Engine-layer entry policy
#
# Owns: LoggingLifetime RAII, terminate handler, single-instance guard,
#       working directory setup, user dialogs, build info.
#
# Consumed PRIVATELY by EXE entry targets only (sfml1_game, sfml1_spatial_harness,
# sfml1_render_stress). Must NOT be linked by any library target — it is not part of
# the engine API surface; it is the bootstrap scaffolding that wraps it.
#
# sfml1_core PRIVATE: entry modules use core::log API, utf8_to_wide, debug_config.
# EXE targets already obtain sfml1_core transitively via sfml1_runtime / sfml1_skyguard —
# no duplication in the final link graph; the linker deduplicates.
#
# user32 PRIVATE (Win32): MessageBoxW (user_dialog), OutputDebugStringW (terminate_handler).
# user32 is also pulled transitively via SFML::Window/SFML::Graphics at EXE link time
# and via sfml1_panic_sink_default — no duplication concern; same pattern as the latter.
#
# TODO(macOS): verify on first macOS CI build; if linker reports unresolved
# symbol(s), fix according to the exact linker error.
# -------------------------------------------------------------------------------------------------
add_library(sfml1_runtime_entry STATIC)

target_link_libraries(sfml1_runtime_entry
    PUBLIC
        sfml1::build_options
    PRIVATE
        sfml1_core
)

if(WIN32)
    target_link_libraries(sfml1_runtime_entry PRIVATE user32)
endif()

target_sources(sfml1_runtime_entry PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/runtime/entry/logging_lifetime.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/runtime/entry/terminate_handler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/runtime/entry/single_instance_guard.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/runtime/entry/working_directory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/runtime/entry/user_dialog.cpp
)

target_precompile_headers(sfml1_runtime_entry PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/pch.h>"
)

# -------------------------------------------------------------------------------------------------
# sfml1_skyguard  —  SkyGuard game runtime (pure game layer, no dev tooling)
# -------------------------------------------------------------------------------------------------
add_library(sfml1_skyguard STATIC)

target_link_libraries(sfml1_skyguard
    PUBLIC
        sfml1::build_options
        sfml1_runtime
        SFML::Graphics
        SFML::Window
        $<$<BOOL:${SFML1_ENABLE_AUDIO}>:sfml1_sfml_audio>
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

# -------------------------------------------------------------------------------------------------
# sfml1_stress_common  —  Shared stress test infrastructure
#
# Owns stress_runtime_stamp.cpp and stress_chunk_content_provider.cpp.
# Consumed PRIVATELY by sfml1_skyguard_dev and sfml1_spatial_harness_lib.
# Depends only on sfml1_runtime; must not depend on sfml1_skyguard to avoid a cycle
# (both consumers already depend on sfml1_skyguard).
# Physical files are located in src/game/skyguard/dev/; CMake target membership
# defines ownership independently of filesystem location.
# -------------------------------------------------------------------------------------------------
add_library(sfml1_stress_common STATIC)

target_link_libraries(sfml1_stress_common
    PUBLIC
        sfml1::build_options
        sfml1_runtime
)

target_sources(sfml1_stress_common PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/skyguard/dev/stress_runtime_stamp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/skyguard/dev/stress_chunk_content_provider.cpp
)

target_precompile_headers(sfml1_stress_common PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/pch.h>"
)

# -------------------------------------------------------------------------------------------------
# sfml1_skyguard_dev  —  Developer overlay (active in Debug AND Profile)
#
# Provides runtime statistics, spatial metrics, and debug overlays.
# Active in Debug (no optimization) and Profile (full /O2 optimization).
# Absent in Release: excluded by generator expression at sfml1_game link step.
# Source guard: #if !defined(NDEBUG) || defined(SFML1_PROFILE)
#
# sfml1_stress_common is a PRIVATE dependency: its types are implementation details
# of overlay_extras.cpp and do not appear in this target's public interface.
# sfml1_game must therefore name sfml1_stress_common explicitly at its own link step.
# -------------------------------------------------------------------------------------------------
add_library(sfml1_skyguard_dev STATIC)

target_link_libraries(sfml1_skyguard_dev
    PUBLIC
        sfml1::build_options
        sfml1_skyguard
    PRIVATE
        sfml1_stress_common
)

target_sources(sfml1_skyguard_dev PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/skyguard/dev/overlay_extras.cpp
)

target_precompile_headers(sfml1_skyguard_dev PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/pch.h>"
)

# -------------------------------------------------------------------------------------------------
# sfml1_spatial_harness_lib  —  Spatial index correctness + timing harness
#
# Debug:   V1 vs V2 result matching, determinism checks.
# Profile: query/update timing, p95/p99 latency benchmarks.
# Requires sfml1_skyguard for FrameOrchestrator.
# sfml1_stress_common is a PRIVATE dependency: implementation detail of
# spatial_index_harness.cpp; does not appear in this target's public interface.
# -------------------------------------------------------------------------------------------------
add_library(sfml1_spatial_harness_lib STATIC)

target_link_libraries(sfml1_spatial_harness_lib
    PUBLIC
        sfml1::build_options
        sfml1_runtime
        sfml1_skyguard
    PRIVATE
        sfml1_stress_common
)

target_sources(sfml1_spatial_harness_lib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/skyguard/dev/spatial_index_harness.cpp
)

target_precompile_headers(sfml1_spatial_harness_lib PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/pch.h>"
)

# -------------------------------------------------------------------------------------------------
# sfml1_render_stress_lib  —  Render stress test (INTERFACE stub; no sources yet)
# -------------------------------------------------------------------------------------------------
add_library(sfml1_render_stress_lib INTERFACE)

target_link_libraries(sfml1_render_stress_lib
    INTERFACE
        sfml1::build_options
        sfml1_skyguard
)

# -------------------------------------------------------------------------------------------------
# GIT_COMMIT_HASH — short commit hash for build traceability
#
# Applied to EXE entry targets only (entry-layer concern, not a library ABI detail).
# Source-level fallback "#ifndef GIT_COMMIT_HASH / #define GIT_COMMIT_HASH <unknown>"
# in include/core/runtime/entry/build_info.h covers builds without Git.
# -------------------------------------------------------------------------------------------------
find_package(Git QUIET)
if(Git_FOUND)
    execute_process(
        COMMAND             ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY   ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE     _SFML1_GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE     _SFML1_GIT_RESULT
    )
    if(NOT _SFML1_GIT_RESULT EQUAL 0)
        set(_SFML1_GIT_HASH "")
    endif()
    unset(_SFML1_GIT_RESULT)
else()
    set(_SFML1_GIT_HASH "")
endif()

# -------------------------------------------------------------------------------------------------
# Executables
# -------------------------------------------------------------------------------------------------

# -------------------------------------------------------------------------------------------------
# sfml1_game  —  SkyGuard game entry point
# -------------------------------------------------------------------------------------------------
add_executable(sfml1_game
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/skyguard/entry/main_skyguard.cpp
)

# WIN32_EXECUTABLE disables CMake's unconditional /subsystem:console injection;
# subsystem is controlled per-config in target_link_options below.
set_target_properties(sfml1_game PROPERTIES WIN32_EXECUTABLE TRUE)

target_link_libraries(sfml1_game PRIVATE
    sfml1_skyguard
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:Profile>>:sfml1_skyguard_dev>
    # sfml1_stress_common is a PRIVATE dep of sfml1_skyguard_dev; STATIC lib PRIVATE deps
    # are not propagated to EXE consumers — the EXE must name them at its own link step.
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:Profile>>:sfml1_stress_common>
    sfml1_runtime_entry
)

target_precompile_headers(sfml1_game PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/pch.h>"
)

# -------------------------------------------------------------------------------------------------
# sfml1_spatial_harness  —  Spatial index benchmark / correctness harness (console tool)
# -------------------------------------------------------------------------------------------------
add_executable(sfml1_spatial_harness
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/skyguard/entry/main_spatial_harness.cpp
)

target_link_libraries(sfml1_spatial_harness PRIVATE
    sfml1_spatial_harness_lib
    sfml1_runtime_entry
)

target_precompile_headers(sfml1_spatial_harness PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/pch.h>"
)

# -------------------------------------------------------------------------------------------------
# sfml1_render_stress  —  Render stress test (entry point stub; no real code yet)
# -------------------------------------------------------------------------------------------------
add_executable(sfml1_render_stress
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/skyguard/entry/main_render_stress.cpp
)
set_target_properties(sfml1_render_stress PROPERTIES WIN32_EXECUTABLE TRUE)

target_link_libraries(sfml1_render_stress PRIVATE
    sfml1_render_stress_lib
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:Profile>>:sfml1_skyguard_dev>
    sfml1_runtime_entry
)

target_precompile_headers(sfml1_render_stress PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/pch.h>"
)

# -------------------------------------------------------------------------------------------------
# SFML::Main — Windows entry point wrapper
#
# Required for /SUBSYSTEM:WINDOWS (Release + Profile): OS calls WinMain, not main().
# Linked in all configs to match vcxproj (sfml-main[-d].lib in all configurations).
# sfml1_spatial_harness excluded: pure console tool, no WinMain needed.
# -------------------------------------------------------------------------------------------------
target_link_libraries(sfml1_game          PRIVATE SFML::Main)
target_link_libraries(sfml1_render_stress PRIVATE SFML::Main)

# -------------------------------------------------------------------------------------------------
# Shell32 — Windows shell API (SHGetFolderPathW / CSIDL_LOCAL_APPDATA)
#
# Required by: sfml1_skyguard/platform/user_paths.cpp
# Purpose:     resolves %LOCALAPPDATA% for per-user config/settings paths.
#
# Why linked here (EXE targets) and NOT in sfml1_skyguard:
#   CMake does not propagate PRIVATE deps of STATIC libs to EXE consumers.
#   Shell32 is an implementation detail of user_paths.cpp — it must NOT appear
#   in sfml1_skyguard's INTERFACE_LINK_LIBRARIES (that would be a PUBLIC lie).
#   The linker resolves SHGetFolderPathW at final EXE link time, so each EXE
#   must name Shell32 explicitly — same pattern as sfml1_stress_common.
#
# sfml1_spatial_harness: linked for consistency — it also links sfml1_skyguard
#   transitively and could call getUserConfigDir() in future stress scenarios.
# -------------------------------------------------------------------------------------------------
if(WIN32)
    target_link_libraries(sfml1_game            PRIVATE Shell32)
    target_link_libraries(sfml1_render_stress   PRIVATE Shell32)
    target_link_libraries(sfml1_spatial_harness PRIVATE Shell32)
endif()

# -------------------------------------------------------------------------------------------------
# GIT_COMMIT_HASH — apply to EXE targets and clean up
# -------------------------------------------------------------------------------------------------
if(_SFML1_GIT_HASH)
    foreach(_exe IN ITEMS sfml1_game sfml1_spatial_harness sfml1_render_stress)
        target_compile_definitions(${_exe} PRIVATE
            "GIT_COMMIT_HASH=\"${_SFML1_GIT_HASH}\""
        )
    endforeach()
    unset(_exe)
endif()
unset(_SFML1_GIT_HASH)

# -------------------------------------------------------------------------------------------------
# Assets sync — POST_BUILD copy for all EXE targets
#
# EXE output directories do not contain assets by default. setWorkingDirectoryFromExecutable()
# searches upward from the EXE for assets/ and fails when running from the output
# directory directly. copy_directory_if_different is timestamp-based: only changed
# files are copied, so incremental builds pay negligible cost.
# -------------------------------------------------------------------------------------------------
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    message(WARNING
        "SFML1: assets directory not found: '${CMAKE_CURRENT_SOURCE_DIR}/assets'. "
        "POST_BUILD sync skipped. EXE targets will load default configs.")
else()
    set(_assets_src "${CMAKE_CURRENT_SOURCE_DIR}/assets")

    foreach(_exe IN ITEMS sfml1_game sfml1_spatial_harness sfml1_render_stress)
        add_custom_command(TARGET ${_exe} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
                "${_assets_src}"
                "$<TARGET_FILE_DIR:${_exe}>/assets"
            COMMENT "Assets sync -> ${_exe}: $<TARGET_FILE_DIR:${_exe}>/assets"
        )
    endforeach()

    unset(_assets_src)
    unset(_exe)
endif()

# -------------------------------------------------------------------------------------------------
# Linker options (MSVC only)
# Source of truth: vcxproj <Link> section per config.
# -------------------------------------------------------------------------------------------------
if(MSVC)

    target_link_options(sfml1_game PRIVATE
        $<$<CONFIG:Debug>:/SUBSYSTEM:CONSOLE>
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:/SUBSYSTEM:WINDOWS>
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:/LTCG>
        $<$<CONFIG:Profile>:/INCREMENTAL:NO>
    )

    target_link_options(sfml1_render_stress PRIVATE
        $<$<CONFIG:Debug>:/SUBSYSTEM:CONSOLE>
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:/SUBSYSTEM:WINDOWS>
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:/LTCG>
        $<$<CONFIG:Profile>:/INCREMENTAL:NO>
    )

    # sfml1_spatial_harness: always console, no render window.
    target_link_options(sfml1_spatial_harness PRIVATE
        /SUBSYSTEM:CONSOLE
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:/LTCG>
        $<$<CONFIG:Profile>:/INCREMENTAL:NO>
    )

endif() # MSVC

# -------------------------------------------------------------------------------------------------
# Stress preset custom targets
#
# One-click stress runs from MSVC Solution Explorer or CLI:
#   cmake --build --preset win-profile --target stress_small
#   cmake --build --preset win-profile --target stress_large
#
# Configuration is read from .env files (12-factor). cmake/run_with_env.cmake provides
# a portable env-file runner without shell dependency. USES_TERMINAL routes output
# directly to the MSVC Output window.
# -------------------------------------------------------------------------------------------------
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