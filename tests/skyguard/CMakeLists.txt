# ================================================================================================
# File: tests/skyguard/CMakeLists.txt
#
# Google Test executable for the game::skyguard layer.
# Currently contains background tiling math tests (migrated from sfml1_skyguard_dev).
#
# Dependency contract:
#   sfml1_skyguard_tests → sfml1_skyguard    (brings include/ + sfml1_core transitively)
#   sfml1_skyguard_tests → GTest::gtest_main
#   sfml1_skyguard_tests does NOT depend on sfml1_skyguard_dev or any tools/stress target.
#
# MSVC warning suppressions: same rationale as tests/engine/CMakeLists.txt.
# ================================================================================================

add_executable(sfml1_skyguard_tests
    src/background_math_test.cpp
)

target_link_libraries(sfml1_skyguard_tests PRIVATE
    sfml1_skyguard     # PUBLIC dep chain: sfml1_core → sfml1::build_options + include paths
    GTest::gtest_main
)

if(MSVC)
    target_compile_options(sfml1_skyguard_tests PRIVATE
        /wd4514  # unreferenced inline function removed (our headers under /Wall)
        /wd4625  # copy constructor implicitly defined as deleted
        /wd4626  # assignment operator implicitly defined as deleted
        /wd5026  # move constructor implicitly defined as deleted
        /wd5027  # move assignment operator implicitly defined as deleted
        /wd4820  # padding bytes added after struct member
        /wd5045  # Spectre mitigation informational
        /wd6326  # SAL: possible comparison of constant with constant (GTest EXPECT_* macros)
    )
endif()

include(GoogleTest)
gtest_discover_tests(sfml1_skyguard_tests)