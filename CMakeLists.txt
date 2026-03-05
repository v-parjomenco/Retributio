# =================================================================================================
# File: CMakeLists.txt
# Purpose: Retributio Engine — solution root and single source of truth for toolchain policy.
# Manages: MSVC runtime, Profile config, imported target mapping, find_package calls.
# =================================================================================================

cmake_minimum_required(VERSION 4.2.3)
project(retributio LANGUAGES C CXX)

# Стандарты языков — до создания любых таргетов.
# C17  — требуется для third_party/xxhash/upstream/xxhash.c
# C++20 — требуется для движка и всех игр
set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)
set(CMAKE_C_STANDARD            17)
set(CMAKE_C_STANDARD_REQUIRED   ON)
set(CMAKE_C_EXTENSIONS          OFF)

# -------------------------------------------------------------------------------------------------
# MSVC debug information format
#
# Предотвращает инъекцию /Zi в CMAKE_CXX_FLAGS_DEBUG (D9025 при /ZI).
# Должно быть установлено до создания любых таргетов.
# -------------------------------------------------------------------------------------------------
if(MSVC)
    string(CONCAT CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
        "$<$<CONFIG:Debug>:EditAndContinue>"
        "$<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:ProgramDatabase>"
    )
endif()

# -------------------------------------------------------------------------------------------------
# MSVC runtime library
#
# Debug            : /MDd  (MultiThreadedDebugDLL)
# Release / Profile: /MD   (MultiThreadedDLL)
#
# Статическая CRT (/MT) создаёт несколько heap-экземпляров при наличии любой DLL,
# что приводит к heap corruption при cross-module alloc/free.
# Устанавливается до создания любых таргетов — в том числе в tests/.
# -------------------------------------------------------------------------------------------------
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# -------------------------------------------------------------------------------------------------
# Profile конфигурация
#
# CMake не знает о кастомной конфигурации "Profile" и молча копирует Release-дефолты.
# Сброс предотвращает дублирование флагов. Все флаги определяются через
# retributio_build_options в engine/CMakeLists.txt.
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
    unset(_v)
endif()

# -------------------------------------------------------------------------------------------------
# Маппинг imported-конфигов
#
# Единственный источник истины: только здесь, до всех find_package и add_subdirectory.
# -------------------------------------------------------------------------------------------------
set(CMAKE_MAP_IMPORTED_CONFIG_PROFILE "Release" CACHE STRING
    "Map imported targets for Profile config to Release" FORCE)

message(STATUS
    "retributio: CMAKE_MAP_IMPORTED_CONFIG_PROFILE = ${CMAKE_MAP_IMPORTED_CONFIG_PROFILE}")

# -------------------------------------------------------------------------------------------------
# Зависимости
# -------------------------------------------------------------------------------------------------
option(RETRIBUTIO_ENABLE_AUDIO "Enable SFML audio module" ON)

if(RETRIBUTIO_ENABLE_AUDIO)
    find_package(SFML CONFIG REQUIRED COMPONENTS Network Graphics Window Audio System Main)
else()
    find_package(SFML CONFIG REQUIRED COMPONENTS Network Graphics Window System Main)
endif()

# -------------------------------------------------------------------------------------------------
# Исправление config-less imported targets (Vorbis, FLAC)
#
# vcpkg создаёт таргеты с IMPORTED_LOCATION (без config-суффикса) вместо
# IMPORTED_LOCATION_<CONFIG>. Ninja Multi-Config отклоняет такие таргеты для Profile.
# MAP_IMPORTED_CONFIG_PROFILE решает link-time, но не generator-validation.
# Решение: явно переводим каждый таргет в per-config режим.
# -------------------------------------------------------------------------------------------------
foreach(_t IN ITEMS Ogg::ogg Vorbis::vorbis Vorbis::vorbisfile Vorbis::vorbisenc FLAC::FLAC)
    if(NOT TARGET ${_t})
        continue()
    endif()
    get_target_property(_loc_debug ${_t} IMPORTED_LOCATION)
    if(NOT _loc_debug)
        continue()
    endif()
    # vcpkg layout: debug libs под .../debug/lib/, release libs под .../lib/
    string(REPLACE "/debug/lib/" "/lib/" _loc_release "${_loc_debug}")
    if(NOT EXISTS "${_loc_release}")
        message(WARNING "retributio: release lib not found at '${_loc_release}', "
            "falling back to debug lib for ${_t}")
        set(_loc_release "${_loc_debug}")
    endif()
    set_target_properties(${_t} PROPERTIES
        IMPORTED_CONFIGURATIONS   "DEBUG;RELEASE;PROFILE"
        IMPORTED_LOCATION_DEBUG   "${_loc_debug}"
        IMPORTED_LOCATION_RELEASE "${_loc_release}"
        IMPORTED_LOCATION_PROFILE "${_loc_release}"
    )
    message(STATUS "retributio: patched ${_t}")
    message(STATUS "  IMPORTED_LOCATION_DEBUG   = ${_loc_debug}")
    message(STATUS "  IMPORTED_LOCATION_RELEASE = ${_loc_release}")
    message(STATUS "  IMPORTED_LOCATION_PROFILE = ${_loc_release}")
endforeach()
unset(_t)
unset(_loc_debug)
unset(_loc_release)

# -------------------------------------------------------------------------------------------------
# Git commit hash
#
# Вычисляется один раз; EXE-таргеты используют переменную RETRIBUTIO_GIT_HASH из root scope.
# -------------------------------------------------------------------------------------------------
find_package(Git QUIET)
if(Git_FOUND)
    execute_process(
        COMMAND             ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY   ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE     RETRIBUTIO_GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE     _git_result
    )
    if(NOT _git_result EQUAL 0)
        set(RETRIBUTIO_GIT_HASH "")
    endif()
    unset(_git_result)
else()
    set(RETRIBUTIO_GIT_HASH "")
endif()

# -------------------------------------------------------------------------------------------------
# Подпроекты
# -------------------------------------------------------------------------------------------------
add_subdirectory(engine)

option(BUILD_ATRAPACIELOS "Build Atrapacielos game" ON)
if(BUILD_ATRAPACIELOS)
    add_subdirectory(games/atrapacielos)
endif()

option(BUILD_AUCTORITAS "Build Auctoritas game" OFF)
if(BUILD_AUCTORITAS)
    add_subdirectory(games/auctoritas)
endif()

option(BUILD_TOOLS "Build dev tools" ON)
if(BUILD_TOOLS)
    add_subdirectory(tools)
endif()

# -------------------------------------------------------------------------------------------------
# Google Tests
#
# enable_testing() в root scope — иначе CTest не видит тесты из поддиректорий.
# find_package(GTest) здесь: tests/ не должен знать о vcpkg деталях.
# -------------------------------------------------------------------------------------------------
option(BUILD_TESTS "Build Google Tests (requires GTest via vcpkg)" ON)
if(BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    add_subdirectory(tests)
endif()