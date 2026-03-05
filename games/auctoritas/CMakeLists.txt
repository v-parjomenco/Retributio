# =================================================================================================
# File: games/auctoritas/CMakeLists.txt
# Purpose: Auctoritas game targets. Included from root via add_subdirectory(games/auctoritas).
#
# Invariant: retributio_auctoritas_game must not include or link anything from games/atrapacielos.
# Depends on retributio_runtime as a library dependency, not as a game peer.
# =================================================================================================

set(_pch_h "${CMAKE_SOURCE_DIR}/engine/include/pch.h")

add_executable(retributio_auctoritas_game
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main_auctoritas.cpp
)

set_target_properties(retributio_auctoritas_game PROPERTIES WIN32_EXECUTABLE TRUE)

target_link_libraries(retributio_auctoritas_game PRIVATE
    retributio_runtime
    retributio_runtime_entry
)

target_precompile_headers(retributio_auctoritas_game PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${_pch_h}>"
)

target_link_libraries(retributio_auctoritas_game PRIVATE SFML::Main)

if(WIN32)
    target_link_libraries(retributio_auctoritas_game PRIVATE Shell32)
endif()

if(RETRIBUTIO_GIT_HASH)
    target_compile_definitions(retributio_auctoritas_game PRIVATE
        "GIT_COMMIT_HASH=\"${RETRIBUTIO_GIT_HASH}\"")
endif()

if(MSVC)
    target_link_options(retributio_auctoritas_game PRIVATE
        $<$<CONFIG:Debug>:/SUBSYSTEM:CONSOLE>
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:/SUBSYSTEM:WINDOWS>
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:/LTCG>
        $<$<CONFIG:Profile>:/INCREMENTAL:NO>
    )
endif()

set(_engine_assets "${CMAKE_SOURCE_DIR}/engine/assets")
if(EXISTS "${_engine_assets}")
    add_custom_command(TARGET retributio_auctoritas_game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            "${_engine_assets}"
            "$<TARGET_FILE_DIR:retributio_auctoritas_game>/assets"
        COMMENT "Assets sync -> retributio_auctoritas_game: engine assets"
    )
endif()

unset(_engine_assets)
unset(_pch_h)