cmake_minimum_required(VERSION 4.2.3)
project(sfml1_root LANGUAGES C CXX)

# ------------------------------------------------------------------------------------------------
# MSVC Runtime Library (project-wide — must be set before ANY target is created)
#
# Mirrors the setting in SFML1/CMakeLists.txt, but declared here in root scope so that
# targets created in tests/ (which are subdirectories of root, not of SFML1/) also
# inherit the correct runtime. SFML1/CMakeLists.txt re-sets the same value — harmless.
#
# Debug   : /MDd   (MultiThreadedDebugDLL)
# Release / Profile  : /MD    (MultiThreadedDLL)
# ------------------------------------------------------------------------------------------------
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY
        "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# ------------------------------------------------------------------------------------------------
# MSVC debug information format (project-wide — same reasoning as MSVC runtime above)
#
# Prevents CMake from injecting /Zi into CMAKE_CXX_FLAGS_DEBUG (causes D9025 with /ZI).
# See SFML1/CMakeLists.txt for the full rationale. Must be set before any target creation.
# ------------------------------------------------------------------------------------------------
if(MSVC)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
        "$<$<CONFIG:Debug>:EditAndContinue>$<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:ProgramDatabase>"
    )
endif()

# ------------------------------------------------------------------------------------------------
# Game + engine targets
# ------------------------------------------------------------------------------------------------
add_subdirectory(SFML1)

# ------------------------------------------------------------------------------------------------
# Google Tests
#
# Enabled by default. Disable with -DBUILD_TESTS=OFF when doing a pure release build
# that must not pull in GTest as a dependency at all.
#
# enable_testing() must be called in the root scope (not just in tests/) so that
# CTest discovers all tests regardless of which subdirectory they are defined in.
# ------------------------------------------------------------------------------------------------
option(BUILD_TESTS "Build Google Tests (requires GTest via vcpkg)" ON)
if(BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    add_subdirectory(tests)
endif()