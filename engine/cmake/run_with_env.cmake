# ================================================================================================
# run_with_env.cmake — Portable env-file runner
#
# Usage (cmake -P):
#   cmake -DENV_FILE=<abs-path-to-.env> -DEXE=<abs-path-to-binary> -P run_with_env.cmake
#
# Reads KEY=VALUE lines from ENV_FILE (skips blank lines and # comments),
# injects them into the child process environment, then executes EXE.
#
# Requires CMake 4.2.3+. No platform-specific shell required.
# ================================================================================================

if(NOT DEFINED ENV_FILE)
    message(FATAL_ERROR "run_with_env.cmake: ENV_FILE is not defined")
endif()
if(NOT DEFINED EXE)
    message(FATAL_ERROR "run_with_env.cmake: EXE is not defined")
endif()
if(NOT EXISTS "${ENV_FILE}")
    message(FATAL_ERROR "run_with_env.cmake: env file not found: ${ENV_FILE}")
endif()
if(NOT EXISTS "${EXE}")
    message(FATAL_ERROR "run_with_env.cmake: executable not found: ${EXE}")
endif()

file(STRINGS "${ENV_FILE}" _env_lines)

foreach(_line IN LISTS _env_lines)
    # Skip blank lines and comment lines
    if(_line STREQUAL "" OR _line MATCHES "^[ \t]*#")
        continue()
    endif()

    # Parse KEY=VALUE — value may itself contain '=' signs (e.g. base64, URLs)
    string(REGEX MATCH "^([^=]+)=(.*)" _matched "${_line}")
    if(NOT _matched)
        message(WARNING "run_with_env.cmake: skipping malformed line: ${_line}")
        continue()
    endif()

    set(_key   "${CMAKE_MATCH_1}")
    set(_value "${CMAKE_MATCH_2}")

    # Strip leading/trailing whitespace from key only; values are preserved verbatim
    string(STRIP "${_key}" _key)

    set(ENV{${_key}} "${_value}")
    message(STATUS "  env: ${_key}=${_value}")
endforeach()

message(STATUS "run_with_env.cmake: launching ${EXE}")

execute_process(
    COMMAND "${EXE}"
    RESULT_VARIABLE _exit_code
)

if(NOT _exit_code EQUAL 0)
    message(FATAL_ERROR "run_with_env.cmake: process exited with code ${_exit_code}")
endif()
