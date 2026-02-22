# ================================================================================================
# File: tests/engine/CMakeLists.txt
#
# Google Test executable for sfml1_core (engine layer).
#
# Dependency contract:
#   sfml1_engine_tests -> sfml1_core           (engine layer only, no game dep)
#   sfml1_engine_tests -> GTest::gtest_main    (GTest runner + main())
#   sfml1_engine_tests does NOT depend on sfml1_runtime, sfml1_skyguard, or any tool.
#
# Link-seam units (mirrors the panic_sink pattern — sfml1_core declares, does not define):
#
#   panic_sink seam:
#     log_panic_sink_throw.cpp — throws std::runtime_error instead of exiting the process.
#     Allows panic paths to be tested via expectPanic<F>().
#     Must NOT coexist with sfml1_panic_sink_default.
#
#   stable_key hash seam:
#     stable_key_impl_seam.cpp — provides core::resources::detail::hashCanonicalKey with an
#     injectable function pointer (defaults to real xxHash3_64). Exposes setHashOverride() so
#     StableKey64 collision detection can be exercised without contriving real collisions.
#     Must NOT coexist with stable_key_impl_xxhash.cpp in the same target.
#
#   Production counterpart (stable_key_impl_xxhash.cpp):
#     This file is NOT listed here. It must be added to sfml1_runtime and any other
#     production executable that links sfml1_core. If you add a new tool target, remember
#     to link it there as well. Failing to do so is a linker error (unresolved symbol:
#     core::resources::detail::hashCanonicalKey).
#
# MSVC warning suppressions:
#   /wd4625, /wd4626, /wd5026, /wd5027 — GTest TEST() macro expands to a class whose
#     copy/move constructors and assignment operators are implicitly deleted.
#     Not actionable — suppress to avoid /WX failures inherited from sfml1::build_options.
#   /wd4820 — padding bytes added in GTest-generated structs (third-party pattern).
#   /wd4514 — unreferenced inline function removed (our headers under /Wall).
#   /wd5045 — Spectre mitigation informational; zero actionability in test code.
#   /wd6326 — SAL: possible comparison of constant with constant (GTest EXPECT_* macros).
#
# PCH:
#   Test files do NOT use the project PCH. Each file includes only what it needs.
#   This keeps tests self-contained and free of PCH-path coupling to SFML1/.
# ================================================================================================

add_executable(sfml1_engine_tests
    src/log_panic_sink_throw.cpp
    src/stable_key_impl_seam.cpp           # seam хэша — заменяет stable_key_impl_xxhash.cpp
    src/string_pool_test.cpp
    src/resource_registry_test.cpp
    src/resource_manager_test.cpp
)

target_include_directories(sfml1_engine_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include    # открывает stable_key_seam.h тестовым TU
)

target_link_libraries(sfml1_engine_tests PRIVATE
    sfml1_core         # engine layer; тянет sfml1::build_options (PUBLIC) + SFML
    GTest::gtest_main  # GTest runner + main()
)

if(MSVC)
    target_compile_options(sfml1_engine_tests PRIVATE
        /wd4514  # удалена неиспользуемая inline-функция
        /wd4625  # конструктор копирования неявно удалён (fixture-классы GTest)
        /wd4626  # оператор присваивания неявно удалён
        /wd5026  # конструктор перемещения неявно удалён
        /wd5027  # оператор перемещения неявно удалён
        /wd4820  # padding-байты после члена структуры (внутренности GTest)
        /wd5045  # информационное предупреждение Spectre mitigation
        /wd6326  # SAL: возможное сравнение константы с константой (EXPECT_* GTest)
    )
endif()

# gtest_discover_tests: запускает бинарник с --gtest_list_tests во время сборки
# и регистрирует каждый TEST() / TEST_F() как отдельную CTest-запись.
include(GoogleTest)
gtest_discover_tests(sfml1_engine_tests)