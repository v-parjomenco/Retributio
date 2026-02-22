// ================================================================================================
// File: tests/skyguard/src/background_math_test.cpp
// Purpose: Google Tests for background tiling math (computeTileStartY, computeTileCount).
// Used by: sfml1_skyguard_tests (CTest, Debug config only)
// Related headers: game/skyguard/presentation/background_math.h
// Notes:
//  - Migrated from background_math_self_test.cpp (SFML1_TESTS static initializer pattern).
//  - Pure math tests — no SFML state, no file I/O, no test hooks required.
//  - No PCH — all includes are explicit.
// ================================================================================================

#include <gtest/gtest.h>

#include "game/skyguard/presentation/background_math.h"

namespace {

    using game::skyguard::presentation::computeTileCount;
    using game::skyguard::presentation::computeTileStartY;

    constexpr float kEps = 1e-4f;

    // Проверяет инвариант: v0 = visibleTop - startY ∈ [0, tileHeight).
    // Вызывается в каждом тесте computeTileStartY — инвариант обязан держаться всегда.
    void expectV0InRange(float visibleTop, float startY, float tileHeight) {
        const float v0 = visibleTop - startY;
        EXPECT_GE(v0, 0.0f)       << "v0 не должен быть отрицательным";
        EXPECT_LT(v0, tileHeight) << "v0 должен быть строго меньше tileHeight";
    }

    // ============================================================================================
    // computeTileStartY
    // ============================================================================================

    TEST(BackgroundMathTest, TileStartY_VisibleTopAlignedToTileBoundary_NoOvershoot) {
        // visibleTop = 128.0 совпадает с границей тайла → startY = 128, v0 = 0.
        constexpr float kVisibleTop = 128.0f;
        constexpr float kOffset     = 0.0f;
        constexpr float kTileHeight = 64.0f;

        const float startY = computeTileStartY(kVisibleTop, kOffset, kTileHeight);

        EXPECT_FLOAT_EQ(startY,               128.0f);
        EXPECT_FLOAT_EQ(kVisibleTop - startY, 0.0f);
        expectV0InRange(kVisibleTop, startY, kTileHeight);
    }

    TEST(BackgroundMathTest, TileStartY_NegativeVisibleTop_StartsOnCorrectTile) {
        // visibleTop = -50 → startY = -64, v0 = 14.
        constexpr float kVisibleTop = -50.0f;
        constexpr float kOffset     = 0.0f;
        constexpr float kTileHeight = 64.0f;

        const float startY = computeTileStartY(kVisibleTop, kOffset, kTileHeight);

        EXPECT_FLOAT_EQ(startY,               -64.0f);
        EXPECT_FLOAT_EQ(kVisibleTop - startY, 14.0f);
        expectV0InRange(kVisibleTop, startY, kTileHeight);
    }

    TEST(BackgroundMathTest, TileStartY_OffsetApplied) {
        // visibleTop = -150, offset = 30, tileHeight = 64 → startY = -162, v0 = 12.
        constexpr float kVisibleTop = -150.0f;
        constexpr float kOffset     = 30.0f;
        constexpr float kTileHeight = 64.0f;

        const float startY = computeTileStartY(kVisibleTop, kOffset, kTileHeight);

        EXPECT_FLOAT_EQ(startY,               -162.0f);
        EXPECT_FLOAT_EQ(kVisibleTop - startY, 12.0f);
        expectV0InRange(kVisibleTop, startY, kTileHeight);
    }

    TEST(BackgroundMathTest, TileStartY_FractionalVisibleTop_V0InRange) {
        // visibleTop = -0.5 → startY ≈ -64, v0 ≈ 63.5.
        // Используем EXPECT_NEAR, т.к. значения нецелые.
        constexpr float kVisibleTop = -0.5f;
        constexpr float kOffset     = 0.0f;
        constexpr float kTileHeight = 64.0f;

        const float startY = computeTileStartY(kVisibleTop, kOffset, kTileHeight);

        EXPECT_NEAR(startY,               -64.0f, kEps);
        EXPECT_NEAR(kVisibleTop - startY, 63.5f,  kEps);
        expectV0InRange(kVisibleTop, startY, kTileHeight);
    }

    // ============================================================================================
    // computeTileCount
    // ============================================================================================

    TEST(BackgroundMathTest, TileCount_ExactFit_NoExtraTile) {
        // 128 / 64 = 2.0 → ровно 2 тайла, округление вверх не нужно.
        EXPECT_EQ(computeTileCount(128.0f, 64.0f), 2);
    }

    TEST(BackgroundMathTest, TileCount_PartialLastTile_RoundsUp) {
        // 100 / 64 ≈ 1.56 → 2 тайла: последний частичный требует дополнительного тайла.
        EXPECT_EQ(computeTileCount(100.0f, 64.0f), 2);
    }

} // namespace