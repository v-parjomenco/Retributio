// ================================================================================================
// File: game/skyguard/presentation/background_math_self_test.cpp
// Purpose: Self-tests for background tiling math (SFML1_TESTS only).
// ================================================================================================
#include "pch.h"

#if defined(SFML1_TESTS)

#include "game/skyguard/presentation/background_math.h"

#include <cassert>
#include <cmath>

namespace game::skyguard::presentation::self_test {

    namespace {

        constexpr float kEps = 1e-4f;

        bool almostEqual(float a, float b, float eps = kEps) noexcept {
            return std::fabs(a - b) <= eps;
        }

        void assertV0InRange(float v0, float tileHeight) noexcept {
            assert(v0 >= 0.0f);
            assert(v0 < tileHeight);
        }

        void testPositiveVisibleTop() {
            const float visibleTop = 128.0f;
            const float offset = 0.0f;
            const float tileHeight = 64.0f;

            const float startY = computeTileStartY(visibleTop, offset, tileHeight);
            assert(startY == 128.0f);

            const float v0 = visibleTop - startY;
            assert(v0 == 0.0f);
            assertV0InRange(v0, tileHeight);
        }

        void testNegativeVisibleTop() {
            const float visibleTop = -50.0f;
            const float offset = 0.0f;
            const float tileHeight = 64.0f;

            const float startY = computeTileStartY(visibleTop, offset, tileHeight);
            assert(startY == -64.0f);

            const float v0 = visibleTop - startY;
            assert(v0 == 14.0f);
            assertV0InRange(v0, tileHeight);
        }

        void testOffsetApplied() {
            const float visibleTop = -150.0f;
            const float offset = 30.0f;
            const float tileHeight = 64.0f;

            const float startY = computeTileStartY(visibleTop, offset, tileHeight);
            assert(startY == -162.0f);

            const float v0 = visibleTop - startY;
            assert(v0 == 12.0f);
            assertV0InRange(v0, tileHeight);
        }

        void testTileCountExactFit() {
            const float visibleHeight = 128.0f;
            const float tileHeight = 64.0f;

            const int tiles = computeTileCount(visibleHeight, tileHeight);
            assert(tiles == 2);
        }

        void testTileCountPartialTile() {
            const float visibleHeight = 100.0f;
            const float tileHeight = 64.0f;

            const int tiles = computeTileCount(visibleHeight, tileHeight);
            assert(tiles == 2);
        }

        void testFractionalVisibleTop() {
            const float visibleTop = -0.5f;
            const float offset = 0.0f;
            const float tileHeight = 64.0f;

            const float startY = computeTileStartY(visibleTop, offset, tileHeight);
            assert(almostEqual(startY, -64.0f));

            const float v0 = visibleTop - startY;
            assert(almostEqual(v0, 63.5f));
            assertV0InRange(v0, tileHeight);
        }

    } // namespace

    void run() {
        testPositiveVisibleTop();
        testNegativeVisibleTop();
        testOffsetApplied();
        testTileCountExactFit();
        testTileCountPartialTile();
        testFractionalVisibleTop();
    }

} // namespace game::skyguard::presentation::self_test

namespace {

    struct SelfTestRunner {
        SelfTestRunner() {
            game::skyguard::presentation::self_test::run();
        }
    } gSelfTestRunner;

} // namespace

#endif // defined(SFML1_TESTS)