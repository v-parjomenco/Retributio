# =================================================================================================
# File: tests/engine/CMakeLists.txt
# Purpose: Google Test executable for retributio_core (engine layer only).
#
# Dependency contract:
#   retributio_engine_tests -> retributio_core
#   retributio_engine_tests -> GTest::gtest_main
#   retributio_engine_tests does NOT depend on retributio_runtime or any game target.
#
# Link-seam units (retributio_core declares but does not define):
#
#   panic_sink seam:
#     log_panic_sink_throw.cpp — throws std::runtime_error instead of terminating the process.
#     Allows testing panic paths via expectPanic<F>().
#     Must NOT coexist with retributio_panic_sink_default.
#
#   stable_key hash seam:
#     stable_key_impl_seam.cpp — provides hashCanonicalKey via an injectable function pointer.
#     Exposes setHashOverride() to exercise StableKey64 collision detection.
#     Must NOT coexist with stable_key_impl_xxhash.cpp in the same target.
#     Missing seam in a production EXE = linker error (unresolved hashCanonicalKey).
#
# MSVC warning suppressions:
#   /wd4625, /wd4626, /wd5026, /wd5027 — TEST() expands to a class with implicitly deleted
#     copy/move — not actionable, suppressed to avoid /WX failures from build_options.
#   /wd4820 — padding bytes in GTest-generated structs (third-party pattern).
#   /wd4514 — unreferenced inline function removed (our headers under /Wall).
#   /wd5045 — Spectre mitigation informational; zero actionability in test code.
#   /wd6326 — SAL: possible comparison of constant with constant (GTest EXPECT_* macros).
#
# PCH is not used: test files are self-contained, no PCH-path coupling to engine/.
# =================================================================================================

add_executable(retributio_engine_tests
    src/log_panic_sink_throw.cpp
    src/stable_key_impl_seam.cpp
    src/string_pool_test.cpp
    src/resource_registry_test.cpp
    src/resource_manager_test.cpp
)

target_include_directories(retributio_engine_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include    # открывает stable_key_seam.h тестовым TU
)

target_link_libraries(retributio_engine_tests PRIVATE
    retributio_core
    GTest::gtest_main
)

if(MSVC)
    target_compile_options(retributio_engine_tests PRIVATE
        /wd4514
        /wd4625
        /wd4626
        /wd5026
        /wd5027
        /wd4820
        /wd5045
        /wd6326
    )
endif()

include(GoogleTest)
gtest_discover_tests(retributio_engine_tests)