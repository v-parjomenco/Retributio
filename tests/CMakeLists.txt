# =================================================================================================
# File: tests/CMakeLists.txt
# Purpose: Google Tests root aggregator.
# Invariants:
#   - tests/ contains only Google Tests; never linked into release game binaries.
#   - tests/engine/ tests retributio_core only (no game deps).
#   - tests/atrapacielos/ tests the atrapacielos game layer.
#   - Run via: ctest --preset win-debug
# =================================================================================================

add_subdirectory(engine)
add_subdirectory(atrapacielos)