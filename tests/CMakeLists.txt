# ================================================================================================
# File: tests/CMakeLists.txt
#
# Google Tests root. Aggregates all test sub-projects.
#
# Invariants (from Architecture ROADMAP):
#   - tests/ contains only Google Tests; never linked into release game binaries.
#   - tests/engine/ tests sfml1_core only (no game/ includes, no sfml1_skyguard dep).
#   - tests/skyguard/ tests game::skyguard layer.
#   - Run via: ctest --preset win-debug
# ================================================================================================

add_subdirectory(engine)
add_subdirectory(skyguard)
