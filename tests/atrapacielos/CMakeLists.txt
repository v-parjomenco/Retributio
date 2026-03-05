# =================================================================================================
# File: tests/atrapacielos/CMakeLists.txt
# Purpose: Google Test executable for the atrapacielos game layer.
#
# Dependency contract:
#   retributio_atrapacielos_tests -> retributio_atrapacielos
#   retributio_atrapacielos_tests -> GTest::gtest_main
#   retributio_atrapacielos_tests does NOT depend on retributio_atrapacielos_dev
#   or any tools target.
#
# MSVC warning suppressions: same rationale as tests/engine/CMakeLists.txt.
# =================================================================================================

add_executable(retributio_atrapacielos_tests
    src/background_math_test.cpp
)

target_link_libraries(retributio_atrapacielos_tests PRIVATE
    retributio_atrapacielos
    GTest::gtest_main
)

if(MSVC)
    target_compile_options(retributio_atrapacielos_tests PRIVATE
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
gtest_discover_tests(retributio_atrapacielos_tests)