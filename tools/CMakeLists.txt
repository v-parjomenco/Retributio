# =================================================================================================
# File: tools/CMakeLists.txt
# Purpose: Dev tools — spatial harness and render stress.
#          Included from root via add_subdirectory(tools) when BUILD_TOOLS=ON.
#          Must NOT appear in any release game binary.
#
# Target graph (A -> B means A depends on B):
#   retributio_spatial_harness_lib -> retributio_runtime, retributio_atrapacielos,
#                                     retributio_stress_common (PRIVATE)
#   retributio_render_stress_lib   -> retributio_atrapacielos  (INTERFACE stub)
#   retributio_spatial_harness     -> retributio_spatial_harness_lib, retributio_runtime_entry
#   retributio_render_stress       -> retributio_render_stress_lib,
#                                     retributio_atrapacielos_dev (Debug+Profile),
#                                     retributio_runtime_entry
# =================================================================================================

set(_pch_h "${CMAKE_SOURCE_DIR}/engine/include/pch.h")

# -------------------------------------------------------------------------------------------------
# retributio_spatial_harness_lib — spatial index harness
#
# Debug:   сравнение результатов V1 vs V2, проверки детерминизма.
# Profile: замеры query/update, p95/p99 latency.
# retributio_stress_common PRIVATE: implementation detail spatial_index_harness.cpp.
# -------------------------------------------------------------------------------------------------
add_library(retributio_spatial_harness_lib STATIC)

target_link_libraries(retributio_spatial_harness_lib
    PUBLIC
        retributio::build_options
        retributio_runtime
        retributio_atrapacielos
    PRIVATE
        retributio_stress_common
)

target_sources(retributio_spatial_harness_lib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/spatial_index_harness.cpp
)

target_precompile_headers(retributio_spatial_harness_lib PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${_pch_h}>"
)

# -------------------------------------------------------------------------------------------------
# retributio_render_stress_lib — render stress (INTERFACE stub; источников нет)
# -------------------------------------------------------------------------------------------------
add_library(retributio_render_stress_lib INTERFACE)

target_link_libraries(retributio_render_stress_lib
    INTERFACE
        retributio::build_options
        retributio_atrapacielos
)

# -------------------------------------------------------------------------------------------------
# retributio_spatial_harness — console tool
# -------------------------------------------------------------------------------------------------
add_executable(retributio_spatial_harness
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main_spatial_harness.cpp
)

target_link_libraries(retributio_spatial_harness PRIVATE
    retributio_spatial_harness_lib
    retributio_runtime_entry
)

target_precompile_headers(retributio_spatial_harness PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${_pch_h}>"
)

if(WIN32)
    target_link_libraries(retributio_spatial_harness PRIVATE Shell32)
endif()

if(RETRIBUTIO_GIT_HASH)
    target_compile_definitions(retributio_spatial_harness PRIVATE
        "GIT_COMMIT_HASH=\"${RETRIBUTIO_GIT_HASH}\"")
endif()

if(MSVC)
    target_link_options(retributio_spatial_harness PRIVATE
        /SUBSYSTEM:CONSOLE
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:/LTCG>
        $<$<CONFIG:Profile>:/INCREMENTAL:NO>
    )
endif()

# -------------------------------------------------------------------------------------------------
# retributio_render_stress — stub entry
# -------------------------------------------------------------------------------------------------
add_executable(retributio_render_stress
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main_render_stress.cpp
)

set_target_properties(retributio_render_stress PROPERTIES WIN32_EXECUTABLE TRUE)

target_link_libraries(retributio_render_stress PRIVATE
    retributio_render_stress_lib
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:Profile>>:retributio_atrapacielos_dev>
    retributio_runtime_entry
)

target_link_libraries(retributio_render_stress PRIVATE SFML::Main)

target_precompile_headers(retributio_render_stress PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${_pch_h}>"
)

if(WIN32)
    target_link_libraries(retributio_render_stress PRIVATE Shell32)
endif()

if(RETRIBUTIO_GIT_HASH)
    target_compile_definitions(retributio_render_stress PRIVATE
        "GIT_COMMIT_HASH=\"${RETRIBUTIO_GIT_HASH}\"")
endif()

if(MSVC)
    target_link_options(retributio_render_stress PRIVATE
        $<$<CONFIG:Debug>:/SUBSYSTEM:CONSOLE>
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:/SUBSYSTEM:WINDOWS>
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:/LTCG>
        $<$<CONFIG:Profile>:/INCREMENTAL:NO>
    )
endif()

# -------------------------------------------------------------------------------------------------
# Assets sync для tool executables
# -------------------------------------------------------------------------------------------------
foreach(_exe IN ITEMS retributio_spatial_harness retributio_render_stress)
    foreach(_assets_src IN ITEMS
            "${CMAKE_SOURCE_DIR}/engine/assets"
            "${CMAKE_SOURCE_DIR}/games/atrapacielos/assets")
        if(NOT EXISTS "${_assets_src}")
            continue()
        endif()
        add_custom_command(TARGET ${_exe} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
                "${_assets_src}"
                "$<TARGET_FILE_DIR:${_exe}>/assets"
            COMMENT "Assets sync -> ${_exe}: ${_assets_src}"
        )
    endforeach()
endforeach()
unset(_assets_src)
unset(_exe)

# -------------------------------------------------------------------------------------------------
# Stress preset custom targets
#
# cmake --build --preset win-profile --target stress_small
# cmake --build --preset win-profile --target stress_large
#
# engine/cmake/run_with_env.cmake — portable env-file runner без shell-зависимости.
# -------------------------------------------------------------------------------------------------
set(_run_with_env "${CMAKE_SOURCE_DIR}/engine/cmake/run_with_env.cmake")
set(_presets_dir  "${CMAKE_CURRENT_SOURCE_DIR}/presets/atrapacielos")

add_custom_target(stress_small
    COMMAND ${CMAKE_COMMAND}
        -DENV_FILE=${_presets_dir}/active_small.env
        -DEXE=$<TARGET_FILE:retributio_spatial_harness>
        -P "${_run_with_env}"
    DEPENDS retributio_spatial_harness
    COMMENT "[stress_small] retributio_spatial_harness + active_small.env"
    USES_TERMINAL
)

add_custom_target(stress_large
    COMMAND ${CMAKE_COMMAND}
        -DENV_FILE=${_presets_dir}/active_large.env
        -DEXE=$<TARGET_FILE:retributio_spatial_harness>
        -P "${_run_with_env}"
    DEPENDS retributio_spatial_harness
    COMMENT "[stress_large] retributio_spatial_harness + active_large.env"
    USES_TERMINAL
)

unset(_run_with_env)
unset(_presets_dir)
unset(_pch_h)