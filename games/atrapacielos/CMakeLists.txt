# =================================================================================================
# File: games/atrapacielos/CMakeLists.txt
# Purpose: Atrapacielos game targets. Included from root via add_subdirectory(games/atrapacielos).
#
# Target graph (A -> B means A depends on B):
#   retributio_atrapacielos      -> retributio_runtime, retributio_sfml_audio (if audio ON),
#                                   SFML::Graphics, SFML::Window, SFML::Network
#   retributio_stress_common     -> retributio_runtime
#   retributio_atrapacielos_dev  -> retributio_atrapacielos,
#                                   retributio_stress_common (PRIVATE)
#   retributio_atrapacielos_game -> retributio_atrapacielos,
#                                   retributio_atrapacielos_dev (Debug+Profile),
#                                   retributio_stress_common (Debug+Profile),
#                                   retributio_runtime_entry
#
# Dev overlay policy (retributio_atrapacielos_dev):
#   Active in Debug and Profile; excluded in Release via generator expression.
#   Source guard: #if !defined(NDEBUG) || defined(RETRIBUTIO_PROFILE)
#
# retributio_stress_common lives here, not in tools/:
#   retributio_atrapacielos_dev depends on it; BUILD_TOOLS=OFF must not break dev overlay.
# =================================================================================================

set(_pch_h "${CMAKE_SOURCE_DIR}/engine/include/pch.h")

# -------------------------------------------------------------------------------------------------
# retributio_atrapacielos — game runtime (чистый game layer, без dev tooling)
# -------------------------------------------------------------------------------------------------
add_library(retributio_atrapacielos STATIC)

target_link_libraries(retributio_atrapacielos
    PUBLIC
        retributio::build_options
        retributio_runtime
        SFML::Graphics
        SFML::Window
        $<$<BOOL:${RETRIBUTIO_ENABLE_AUDIO}>:retributio_sfml_audio>
        SFML::Network
)

target_include_directories(retributio_atrapacielos
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(retributio_atrapacielos PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bootstrap/scene_bootstrap.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/config/loader/app_config_loader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/config/loader/config_loader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/config/loader/spatial_v2_config_builder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/config/loader/user_settings_loader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/config/loader/view_config_loader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/config/loader/window_config_loader.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/ecs/systems/player_init_system.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/game.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/orchestration/frame_orchestrator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/user_paths.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/presentation/background_renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/presentation/view_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/presentation/window_mode_manager.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/debug_format.cpp
)

target_precompile_headers(retributio_atrapacielos PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${_pch_h}>"
)

# -------------------------------------------------------------------------------------------------
# retributio_stress_common — общая stress-инфраструктура для dev builds
#
# Владеет stress_runtime_stamp.cpp и stress_chunk_content_provider.cpp.
# Потребляется ПРИВАТНО retributio_atrapacielos_dev и retributio_spatial_harness_lib.
# НЕ зависит от retributio_atrapacielos: оба потребителя уже от него зависят,
# добавление этой зависимости создало бы цикл.
# -------------------------------------------------------------------------------------------------
add_library(retributio_stress_common STATIC)

target_link_libraries(retributio_stress_common
    PUBLIC
        retributio::build_options
        retributio_runtime
)

target_include_directories(retributio_stress_common
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(retributio_stress_common PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dev/stress_runtime_stamp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dev/stress_chunk_content_provider.cpp
)

target_precompile_headers(retributio_stress_common PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${_pch_h}>"
)

# -------------------------------------------------------------------------------------------------
# retributio_atrapacielos_dev — dev overlay (Debug и Profile)
#
# retributio_stress_common PRIVATE: implementation detail overlay_extras.cpp;
# не появляется в публичном интерфейсе этого таргета.
# retributio_atrapacielos_game поэтому именует retributio_stress_common явно.
# -------------------------------------------------------------------------------------------------
add_library(retributio_atrapacielos_dev STATIC)

target_link_libraries(retributio_atrapacielos_dev
    PUBLIC
        retributio::build_options
        retributio_atrapacielos
    PRIVATE
        retributio_stress_common
)

target_sources(retributio_atrapacielos_dev PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dev/overlay_extras.cpp
)

target_precompile_headers(retributio_atrapacielos_dev PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${_pch_h}>"
)

# -------------------------------------------------------------------------------------------------
# retributio_atrapacielos_game — executable
# -------------------------------------------------------------------------------------------------
add_executable(retributio_atrapacielos_game
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main_atrapacielos.cpp
)

# WIN32_EXECUTABLE подавляет безусловную инъекцию /subsystem:console от CMake;
# subsystem управляется per-config в target_link_options ниже.
set_target_properties(retributio_atrapacielos_game PROPERTIES WIN32_EXECUTABLE TRUE)

target_link_libraries(retributio_atrapacielos_game PRIVATE
    retributio_atrapacielos
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:Profile>>:retributio_atrapacielos_dev>
    # retributio_stress_common — PRIVATE dep retributio_atrapacielos_dev; STATIC PRIVATE deps
    # не пропагируются EXE-потребителю — EXE обязан именовать явно.
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:Profile>>:retributio_stress_common>
    retributio_runtime_entry
)

target_precompile_headers(retributio_atrapacielos_game PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${_pch_h}>"
)

target_link_libraries(retributio_atrapacielos_game PRIVATE SFML::Main)

# Shell32: SHGetFolderPathW (platform/user_paths.cpp).
# Implementation detail user_paths.cpp — не должен появляться в INTERFACE_LINK_LIBRARIES
# game-lib. Линкер разрешает символ при финальной сборке EXE.
if(WIN32)
    target_link_libraries(retributio_atrapacielos_game PRIVATE Shell32)
endif()

if(RETRIBUTIO_GIT_HASH)
    target_compile_definitions(retributio_atrapacielos_game PRIVATE
        "GIT_COMMIT_HASH=\"${RETRIBUTIO_GIT_HASH}\"")
endif()

if(MSVC)
    target_link_options(retributio_atrapacielos_game PRIVATE
        $<$<CONFIG:Debug>:/SUBSYSTEM:CONSOLE>
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:/SUBSYSTEM:WINDOWS>
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:/LTCG>
        $<$<CONFIG:Profile>:/INCREMENTAL:NO>
    )
endif()

# -------------------------------------------------------------------------------------------------
# Assets sync — POST_BUILD копирование для retributio_atrapacielos_game
#
# copy_directory_if_different — timestamp-based: только изменённые файлы копируются.
# Копируем engine assets и game assets раздельно в один assets/ рядом с EXE.
# -------------------------------------------------------------------------------------------------
foreach(_assets_src IN ITEMS
        "${CMAKE_SOURCE_DIR}/engine/assets"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    if(NOT EXISTS "${_assets_src}")
        message(WARNING "retributio: assets not found: '${_assets_src}', sync skipped.")
        continue()
    endif()
    add_custom_command(TARGET retributio_atrapacielos_game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            "${_assets_src}"
            "$<TARGET_FILE_DIR:retributio_atrapacielos_game>/assets"
        COMMENT "Assets sync -> retributio_atrapacielos_game: ${_assets_src}"
    )
endforeach()
unset(_assets_src)
unset(_pch_h)