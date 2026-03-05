# =================================================================================================
# File: engine/CMakeLists.txt
# Purpose: Retributio Engine static libraries. Included from root via add_subdirectory(engine).
# Toolchain / MSVC / Profile policy inherited from root.
#
# Configurations (mirror of vcxproj):
#   Debug    — /Od /ZI /RTC1 /JMC, /MDd, Console subsystem
#              Defines: SFML_STATIC _DEBUG _CONSOLE _UNICODE UNICODE
#   Profile  — /O2 /Ob2 /Ot /GL + /LTCG, /MD, Windows subsystem
#              Defines: SFML_STATIC NDEBUG RETRIBUTIO_PROFILE _UNICODE UNICODE
#              INVARIANT: must NOT define _DEBUG.
#              INVARIANT: full release-like optimization — /O2 /Ob2 /Ot /GL.
#   Release  — /O2 /GL + /LTCG, /MD, Windows subsystem
#              Defines: SFML_STATIC NDEBUG _UNICODE UNICODE
#
# Target graph (A -> B means A depends on B):
#   retributio_core                — ECS, spatial, resources, logging, UI utils.
#                                    Declares panic_sink and hashCanonicalKey; does NOT define.
#   retributio_stable_key_xxhash  — StableKey seam via xxHash3_64. Linked by runtime only.
#   retributio_sfml_audio         — SFML audio backend. SFML::Audio PUBLIC.
#   retributio_panic_sink_default — Production panic: MessageBox + std::exit.
#   retributio_runtime            — INTERFACE: core + panic + StableKey seam.
#   retributio_runtime_entry      — Entry policy (logging, terminate, single-instance, dirs).
#                                    Consumed PRIVATELY by EXE targets only.
# =================================================================================================

# -------------------------------------------------------------------------------------------------
# Per-source overrides
#
# Объявляются до создания таргетов.
# -------------------------------------------------------------------------------------------------
set(_pch_cpp  "${CMAKE_CURRENT_SOURCE_DIR}/src/pch.cpp")
set(_xxhash_c "${CMAKE_SOURCE_DIR}/third_party/xxhash/upstream/xxhash.c")

# pch.cpp присутствует в sources только для видимости в IDE;
# не должен участвовать в CMake PCH механизме (/Yc конфликтует с /Yu).
set_source_files_properties("${_pch_cpp}" PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

# xxhash.c — C-код третьей стороны, несовместим с C++ PCH.
# /W4 /WX- переопределяют project-wide /Wall + COMPILE_WARNING_AS_ERROR.
set_source_files_properties("${_xxhash_c}" PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
if(MSVC)
    # APPEND: флаги следуют за retributio_build_options; MSVC применяет последний /WX[+/-].
    set_property(SOURCE "${_xxhash_c}" APPEND PROPERTY COMPILE_OPTIONS
        /W4 /WX- /wd4710 /wd4711 /wd4820 /wd5045
    )
endif()

# -------------------------------------------------------------------------------------------------
# retributio_build_options — INTERFACE-библиотека флагов компиляции для ВСЕХ таргетов
# retributio::build_options — ALIAS; используй это имя в target_link_libraries
#
# Единственный источник флагов компилятора. Каждый флаг соответствует настройке vcxproj.
# -------------------------------------------------------------------------------------------------
add_library(retributio_build_options INTERFACE)
add_library(retributio::build_options ALIAS retributio_build_options)

target_compile_features(retributio_build_options INTERFACE cxx_std_20)

# Препроцессорные определения — зеркало vcxproj <PreprocessorDefinitions> per config.
target_compile_definitions(retributio_build_options INTERFACE
    SFML_STATIC
    _UNICODE
    UNICODE
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Debug>:_CONSOLE>
    $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:NDEBUG>
    $<$<CONFIG:Profile>:RETRIBUTIO_PROFILE>
)

if(MSVC)
    # Универсальные флаги — все конфигурации, все языки
    target_compile_options(retributio_build_options INTERFACE
        /utf-8
        /permissive-
        /wd4710
        /wd4711
        /sdl
        /GS
        /fp:precise
        /FC
        /diagnostics:column
        /Zc:wchar_t
        /Zc:inline
        /external:anglebrackets
        /external:W0
        /analyze:external-
    )

    # C++-only флаги (все конфигурации)
    target_compile_options(retributio_build_options INTERFACE
        $<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>
        $<$<COMPILE_LANGUAGE:CXX>:/Zc:preprocessor>
        $<$<COMPILE_LANGUAGE:CXX>:/EHsc>
        $<$<COMPILE_LANGUAGE:CXX>:/Wall>
    )

    # C-only флаги (все конфигурации)
    target_compile_options(retributio_build_options INTERFACE
        $<$<COMPILE_LANGUAGE:C>:/Wall>
    )

    # Debug
    target_compile_options(retributio_build_options INTERFACE
        $<$<CONFIG:Debug>:/Od>
        # /ZI инжектируется через CMAKE_MSVC_DEBUG_INFORMATION_FORMAT — не дублировать.
        $<$<CONFIG:Debug>:/RTC1>
        $<$<CONFIG:Debug>:/JMC>
    )

    # Release / Profile — общая оптимизационная база
    target_compile_options(retributio_build_options INTERFACE
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:/O2>
        # /Zi инжектируется через CMAKE_MSVC_DEBUG_INFORMATION_FORMAT — не дублировать.
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:/GL>
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:/Gy>
        $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:/Oi>
    )

    # Profile — дополнительные флаги скорости поверх Release baseline
    target_compile_options(retributio_build_options INTERFACE
        $<$<CONFIG:Profile>:/Ob2>
        $<$<CONFIG:Profile>:/Ot>
    )
endif()

# Наследуется всеми потребителями через build_options.
# xxhash.c переопределяет /WX- per-source (см. блок выше).
set_target_properties(retributio_build_options PROPERTIES COMPILE_WARNING_AS_ERROR ON)

# -------------------------------------------------------------------------------------------------
# retributio_core — engine static library
#
# panic_sink объявлен в logging.h, но НЕ определён здесь.
# Определение — retributio_panic_sink_default (production) или тестовый seam TU.
# retributio_runtime — canonical production комбо.
#
# hashCanonicalKey объявлен в stable_key.h, НЕ определён здесь.
# Production определение — retributio_stable_key_xxhash (через retributio_runtime).
# Тесты предоставляют собственный seam TU и НЕ линкуют retributio_stable_key_xxhash.
# -------------------------------------------------------------------------------------------------
add_library(retributio_core STATIC)

target_link_libraries(retributio_core
    PUBLIC
        retributio::build_options
        SFML::System
        SFML::Window
        SFML::Graphics
)

target_include_directories(retributio_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/third_party
)

target_sources(retributio_core PRIVATE
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

# REUSE_FROM несовместим с Ninja Multi-Config + MSVC (/Fd path conflict).
target_precompile_headers(retributio_core PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/pch.h>"
)

# -------------------------------------------------------------------------------------------------
# retributio_stable_key_xxhash — production StableKey seam
#
# Определяет hashCanonicalKey через xxHash3_64.
# xxHash скомпилирован в retributio_core (xxhash.c выше).
# Production таргеты получают seam через retributio_runtime.
# Тесты НЕ должны линковать этот таргет.
# -------------------------------------------------------------------------------------------------
add_library(retributio_stable_key_xxhash STATIC)

target_link_libraries(retributio_stable_key_xxhash
    PUBLIC  retributio::build_options
    PRIVATE retributio_core
)

target_sources(retributio_stable_key_xxhash PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/resources/keys/stable_key_impl_xxhash.cpp
)

target_precompile_headers(retributio_stable_key_xxhash PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/pch.h>"
)

# -------------------------------------------------------------------------------------------------
# retributio_sfml_audio — SFML audio backend для ISoundResource
#
# Self-sufficient: линкует SFML::Audio PUBLIC — потребитель не именует SFML::Audio сам.
#
# include_private/ изолирует SFML-специфичные implementation headers (sfml_sound_resource.h).
# Публичный API: sfml_audio_loader.h в engine/include/, доступен через retributio_core.
#
# retributio_core PRIVATE: каждый потребитель уже получает core через retributio_runtime.
# Если появится потребитель без retributio_runtime — повысить до PUBLIC.
# -------------------------------------------------------------------------------------------------
if(RETRIBUTIO_ENABLE_AUDIO)
    add_library(retributio_sfml_audio STATIC)

    target_link_libraries(retributio_sfml_audio
        PUBLIC  SFML::Audio
        PRIVATE retributio_core
    )

    target_include_directories(retributio_sfml_audio
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include_private
    )

    target_sources(retributio_sfml_audio PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/backends/sfml_audio/sfml_audio_loader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/backends/sfml_audio/sfml_sound_resource.cpp
    )

    target_precompile_headers(retributio_sfml_audio PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/pch.h>"
    )
endif()

# -------------------------------------------------------------------------------------------------
# retributio_panic_sink_default — production panic policy (MessageBox + std::exit)
#
# Определяет panic_sink, требуемый retributio_core.
# user32 PRIVATE (Win32): MessageBoxW.
# -------------------------------------------------------------------------------------------------
add_library(retributio_panic_sink_default STATIC)

target_link_libraries(retributio_panic_sink_default
    PUBLIC  retributio::build_options
    PRIVATE retributio_core
)

if(WIN32)
    target_link_libraries(retributio_panic_sink_default PRIVATE user32)
endif()

target_include_directories(retributio_panic_sink_default
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(retributio_panic_sink_default PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/log/panic_sink_default.cpp
)

target_precompile_headers(retributio_panic_sink_default PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/pch.h>"
)

# -------------------------------------------------------------------------------------------------
# retributio_runtime — production runtime: core + panic + StableKey seam (без audio)
#
# INTERFACE composition point для всех game-таргетов.
# Audio НЕ входит в минимальный runtime — game-таргеты добавляют retributio_sfml_audio явно.
# Тесты линкуют retributio_core напрямую + собственные seam TU.
# -------------------------------------------------------------------------------------------------
add_library(retributio_runtime INTERFACE)

target_link_libraries(retributio_runtime
    INTERFACE
        retributio_core
        retributio_panic_sink_default
        retributio_stable_key_xxhash
)

# -------------------------------------------------------------------------------------------------
# retributio_runtime_entry — engine-layer entry policy
#
# LoggingLifetime RAII, terminate handler, single-instance guard,
# working directory setup, user dialogs, build info.
#
# Потребляется ПРИВАТНО только EXE-таргетами — не является частью engine API.
# retributio_core PRIVATE: entry modules используют core logging; EXE уже получает core
# транзитивно через game-таргет — дублирования в link graph нет.
# user32 PRIVATE (Win32): MessageBoxW, OutputDebugStringW.
# -------------------------------------------------------------------------------------------------
add_library(retributio_runtime_entry STATIC)

target_link_libraries(retributio_runtime_entry
    PUBLIC  retributio::build_options
    PRIVATE retributio_core
)

if(WIN32)
    target_link_libraries(retributio_runtime_entry PRIVATE user32)
endif()

target_sources(retributio_runtime_entry PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/runtime/entry/logging_lifetime.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/runtime/entry/terminate_handler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/runtime/entry/single_instance_guard.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/runtime/entry/working_directory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/runtime/entry/user_dialog.cpp
)

target_precompile_headers(retributio_runtime_entry PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/pch.h>"
)

unset(_pch_cpp)
unset(_xxhash_c)