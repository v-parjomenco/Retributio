# =================================================================================================
# File: tools/CMakeLists.txt
# Purpose: Dev tools — spatial harness and render stress.
#          Included from root via add_subdirectory(tools) when BUILD_TOOLS=ON.
#          Must NOT appear in any release game binary.
#
# Target graph (A -> B means A depends on B):
#   retributio_stress_spatial_lib -> retributio_runtime, retributio_atrapacielos,
#                                     retributio_stress_common (PRIVATE)
#   retributio_stress_render_lib   -> retributio_atrapacielos  (INTERFACE)
#   retributio_stress_spatial      -> retributio_stress_spatial_lib, retributio_runtime_entry
#   retributio_stress_render       -> retributio_stress_render_lib,
#                                     retributio_atrapacielos_dev (Debug+Profile),
#                                     retributio_runtime_entry
# =================================================================================================

set(_pch_h "${CMAKE_SOURCE_DIR}/engine/include/pch.h")

# -------------------------------------------------------------------------------------------------
# retributio_stress_spatial_lib — spatial index harness
#
# Debug:   сравнение результатов V1 vs V2, проверки детерминизма.
# Profile: замеры query/update, p95/p99 latency.
# retributio_stress_common PRIVATE: implementation detail spatial_index_harness.cpp.
# -------------------------------------------------------------------------------------------------
add_library(retributio_stress_spatial_lib STATIC)

target_link_libraries(retributio_stress_spatial_lib
    PUBLIC
        retributio::build_options
        retributio_runtime
        retributio_atrapacielos
    PRIVATE
        retributio_stress_common
)

target_sources(retributio_stress_spatial_lib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/spatial_index_harness.cpp
)

target_precompile_headers(retributio_stress_spatial_lib PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${_pch_h}>"
)

# -------------------------------------------------------------------------------------------------
# retributio_stress_render_lib — render stress (INTERFACE stub; источников нет)
# -------------------------------------------------------------------------------------------------
add_library(retributio_stress_render_lib INTERFACE)

target_link_libraries(retributio_stress_render_lib
    INTERFACE
        retributio::build_options
        retributio_atrapacielos
)

# -------------------------------------------------------------------------------------------------
# retributio_stress_spatial — console tool
# -------------------------------------------------------------------------------------------------
add_executable(retributio_stress_spatial
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main_stress_spatial.cpp
)

target_link_libraries(retributio_stress_spatial PRIVATE
    retributio_stress_spatial_lib
    retributio_runtime_entry
)

target_precompile_headers(retributio_stress_spatial PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${_pch_h}>"
)

if(WIN32)
    target_link_libraries(retributio_stress_spatial PRIVATE Shell32)
endif()

if(RETRIBUTIO_GIT_HASH)
    target_compile_definitions(retributio_stress_spatial PRIVATE
        "GIT_COMMIT_HASH=\"${RETRIBUTIO_GIT_HASH}\"")
endif()

if(MSVC)
    target_link_options(retributio_stress_spatial PRIVATE
        /SUBSYSTEM:CONSOLE
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:/LTCG>
        $<$<CONFIG:Profile>:/INCREMENTAL:NO>
    )
endif()

# -------------------------------------------------------------------------------------------------
# retributio_stress_render — stub entry
# -------------------------------------------------------------------------------------------------
add_executable(retributio_stress_render
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main_stress_render.cpp
)

set_target_properties(retributio_stress_render PROPERTIES WIN32_EXECUTABLE TRUE)

target_link_libraries(retributio_stress_render PRIVATE
    retributio_stress_render_lib
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:Profile>>:retributio_atrapacielos_dev>
    retributio_runtime_entry
)

target_link_libraries(retributio_stress_render PRIVATE SFML::Main)

target_precompile_headers(retributio_stress_render PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${_pch_h}>"
)

if(WIN32)
    target_link_libraries(retributio_stress_render PRIVATE Shell32)
endif()

if(RETRIBUTIO_GIT_HASH)
    target_compile_definitions(retributio_stress_render PRIVATE
        "GIT_COMMIT_HASH=\"${RETRIBUTIO_GIT_HASH}\"")
endif()

if(MSVC)
    target_link_options(retributio_stress_render PRIVATE
        $<$<CONFIG:Debug>:/SUBSYSTEM:CONSOLE>
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:/SUBSYSTEM:WINDOWS>
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:/LTCG>
        $<$<CONFIG:Profile>:/INCREMENTAL:NO>
    )
endif()

# -------------------------------------------------------------------------------------------------
# Assets sync для tool executables
# -------------------------------------------------------------------------------------------------
foreach(_exe IN ITEMS retributio_stress_spatial retributio_stress_render)
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
# cmake --build --preset win-profile --target spatial_small
# cmake --build --preset win-profile --target spatial_large
# cmake --build --preset win-profile --target render_small
# cmake --build --preset win-profile --target render_large
#
# engine/cmake/run_with_env.cmake — portable env-file runner без shell-зависимости.
# -------------------------------------------------------------------------------------------------
set(_run_with_env "${CMAKE_SOURCE_DIR}/engine/cmake/run_with_env.cmake")
set(_presets_dir  "${CMAKE_CURRENT_SOURCE_DIR}/presets/atrapacielos")

add_custom_target(spatial_small
    COMMAND ${CMAKE_COMMAND}
        -DENV_FILE=${_presets_dir}/stress_spatial/active_small.env
        -DEXE=$<TARGET_FILE:retributio_stress_spatial>
        -P "${_run_with_env}"
    DEPENDS retributio_stress_spatial
    COMMENT "[spatial_small] retributio_stress_spatial + stress_spatial/active_small.env"
    USES_TERMINAL
)

add_custom_target(spatial_large
    COMMAND ${CMAKE_COMMAND}
        -DENV_FILE=${_presets_dir}/stress_spatial/active_large.env
        -DEXE=$<TARGET_FILE:retributio_stress_spatial>
        -P "${_run_with_env}"
    DEPENDS retributio_stress_spatial
    COMMENT "[spatial_large] retributio_stress_spatial + stress_spatial/active_large.env"
    USES_TERMINAL
)

add_custom_target(render_small
    COMMAND ${CMAKE_COMMAND}
        -DENV_FILE=${_presets_dir}/stress_render/render_small.env
        -DEXE=$<TARGET_FILE:retributio_stress_render>
        -P "${_run_with_env}"
    DEPENDS retributio_stress_render
    COMMENT "[render_small] retributio_stress_render + stress_render/render_small.env"
    USES_TERMINAL
)

add_custom_target(render_large
    COMMAND ${CMAKE_COMMAND}
        -DENV_FILE=${_presets_dir}/stress_render/render_large.env
        -DEXE=$<TARGET_FILE:retributio_stress_render>
        -P "${_run_with_env}"
    DEPENDS retributio_stress_render
    COMMENT "[render_large] retributio_stress_render + stress_render/render_large.env"
    USES_TERMINAL
)

unset(_run_with_env)
unset(_presets_dir)
unset(_pch_h)