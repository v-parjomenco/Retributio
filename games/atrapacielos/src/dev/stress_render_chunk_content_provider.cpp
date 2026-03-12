#include "pch.h"

#include "dev/stress_render_chunk_content_provider.h"

#if defined(RETRIBUTIO_PROFILE)

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string_view>

#include "core/log/log_macros.h"
#include "core/resources/resource_manager.h"

#include "dev/stress_env_utils.h"
#include "dev/stress_render_options.h"

namespace env = game::atrapacielos::dev::env;

namespace {

    // ============================================================================================
    // RNG (deterministic, zero-alloc)
    // ============================================================================================

    [[nodiscard]] std::uint64_t mix64(std::uint64_t x) noexcept {
        x += 0x9E3779B97F4A7C15ull;
        x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
        x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
        return x ^ (x >> 31);
    }

    [[nodiscard]] std::uint64_t hashChunkCoord(const core::spatial::ChunkCoord c) noexcept {
        const std::uint64_t ux = static_cast<std::uint32_t>(c.x);
        const std::uint64_t uy = static_cast<std::uint32_t>(c.y);
        return mix64((ux << 32) ^ uy);
    }

    struct XorShift64 final {
        std::uint64_t state = 0xA3C59AC3F0E1D2B5ull;

        [[nodiscard]] std::uint32_t nextU32() noexcept {
            std::uint64_t x = state;
            x ^= (x << 13);
            x ^= (x >> 7);
            x ^= (x << 17);
            state = x;
            return static_cast<std::uint32_t>(x >> 32);
        }
    };

    [[nodiscard]] std::size_t uniformIndex(XorShift64& rng, const std::size_t bound) noexcept {
        const std::uint64_t x = static_cast<std::uint64_t>(rng.nextU32());
        return static_cast<std::size_t>((x * static_cast<std::uint64_t>(bound)) >> 32);
    }

    [[nodiscard]] float nextFloat01(XorShift64& rng) noexcept {
        constexpr float kScale = 1.0f / 4294967296.0f;
        return static_cast<float>(rng.nextU32()) * kScale;
    }

    // Signed float in [-1, +1].
    [[nodiscard]] float nextFloatSigned(XorShift64& rng) noexcept {
        return nextFloat01(rng) * 2.0f - 1.0f;
    }

    [[nodiscard]] std::size_t checkedMul(const std::size_t a, const std::size_t b,
                                         const char* where) {
        if (a != 0u && b > (std::numeric_limits<std::size_t>::max() / a)) {
            LOG_PANIC(core::log::cat::Performance, "Stress render provider: {} overflow ({} * {}).",
                      where, a, b);
        }
        return a * b;
    }

    [[nodiscard]] std::size_t divCeil(const std::size_t num, const std::size_t den) noexcept {
        return (num + den - 1u) / den;
    }

    // ============================================================================================
    // UB-free unsigned absolute value for signed int32.
    //
    // std::abs(INT_MIN) is undefined behavior in C++.
    // This helper casts to uint32 first (well-defined modular conversion in two's complement),
    // then negates via subtraction from zero (well-defined unsigned wraparound).
    //
    // Using (0u - u) instead of (-u) to avoid MSVC C4146 ("unary minus on unsigned type").
    // Semantically identical: both produce two's complement negation on unsigned.
    // ============================================================================================
    [[nodiscard]] std::size_t unsignedAbs32(const std::int32_t v) noexcept {
        const auto u = static_cast<std::uint32_t>(v);
        return static_cast<std::size_t>((v >= 0) ? u : (0u - u));
    }

    // ============================================================================================
    // Zone classification
    //
    // Hotspot center = world chunk (0,0) = player start position.
    // Ring = Chebyshev distance from (0,0): max(|coord.x|, |coord.y|).
    //
    // This is correct for STATIONARY benchmarks:
    //   - Player does not move during stress test.
    //   - Streaming window is centered on the player by SpatialStreamingSystem.
    //   - So the hotspot IS around the camera at rest.
    //   - If the player moves, density does NOT follow. This is intentional:
    //     render stress measures steady-state performance, not dynamic rebalancing.
    // ============================================================================================

    enum class ChunkZone : std::uint8_t { Hotspot, Overscan, Far };

    [[nodiscard]] ChunkZone classifyChunk(const core::spatial::ChunkCoord coord,
                                          const std::size_t hotspotRadius,
                                          const std::size_t overscanMax) noexcept {
        const std::size_t ring = std::max(unsignedAbs32(coord.x), unsignedAbs32(coord.y));
        if (ring <= hotspotRadius) {
            return ChunkZone::Hotspot;
        }
        if (ring <= overscanMax) {
            return ChunkZone::Overscan;
        }
        return ChunkZone::Far;
    }

    // --------------------------------------------------------------------------------------------
    // Count chunks per zone by iterating the ACTUAL predicted window coordinates.
    //
    // Key: the window origin at runtime is NOT (0,0). The streaming system shifts it
    // to (-margin, -margin) because:
    //   1. Config origin = (0,0).
    //   2. Player at chunk (0,0).
    //   3. computeDesiredOrigin: focus(0) < innerMin(0 + margin) → origin = -margin.
    //
    // So we iterate from (predictedOriginX, predictedOriginY) for W×H chunks.
    // This correctly counts chunks with negative coordinates that fall into the hotspot.
    //
    // Example: window 9×9, margin 1 → predicted origin (-1,-1), range (-1,-1) to (7,7).
    //   hotspot (ring ≤ 1): 9 chunks (correct, not 4 as from (0,0)).
    // --------------------------------------------------------------------------------------------
    struct ZoneCounts final {
        std::size_t hotspot = 0;
        std::size_t overscan = 0;
        std::size_t far = 0;
    };

    [[nodiscard]] ZoneCounts countActualZones(
        const std::int32_t predictedOriginX,
        const std::int32_t predictedOriginY,
        const std::int32_t windowWidth,
        const std::int32_t windowHeight,
        const std::size_t hotspotRadius,
        const std::size_t overscanChunks) noexcept {

        const std::size_t overscanMax = hotspotRadius + overscanChunks;
        ZoneCounts counts{};

        for (std::int32_t y = predictedOriginY; y < predictedOriginY + windowHeight; ++y) {
            for (std::int32_t x = predictedOriginX; x < predictedOriginX + windowWidth; ++x) {
                const auto zone = classifyChunk({x, y}, hotspotRadius, overscanMax);
                switch (zone) {
                case ChunkZone::Hotspot:
                    ++counts.hotspot;
                    break;
                case ChunkZone::Overscan:
                    ++counts.overscan;
                    break;
                case ChunkZone::Far:
                    ++counts.far;
                    break;
                }
            }
        }

        return counts;
    }

    // ============================================================================================
    // Texture list from env
    // ============================================================================================

    [[nodiscard]] std::vector<core::resources::TextureKey>
    buildTextureListFromEnv(core::resources::ResourceManager& resources,
                            const core::resources::TextureKey fallbackTexture) {

        constexpr std::size_t kMaxTextureIds = 64;

        std::vector<core::resources::TextureKey> out;
        const std::size_t registryCount = resources.registry().textureCount();

        if (registryCount == 0) {
            if (fallbackTexture.valid()) {
                out.push_back(fallbackTexture);
            }
            return out;
        }

        std::array<char, 1024> idsBuf{};
        const std::string_view ids =
            env::readStringView("ATRAPACIELOS_STRESS_RENDER_TEXTURE_IDS", idsBuf);

        if (!ids.empty()) {
            out.reserve(std::min(kMaxTextureIds, registryCount));
            env::parseCsvTokens(ids, [&](std::string_view token) noexcept -> bool {
                if (out.size() >= kMaxTextureIds) {
                    return false;
                }
                std::uint32_t idx = 0;
                if (env::parseU32(token, idx) && (static_cast<std::size_t>(idx) < registryCount)) {
                    out.push_back(core::resources::TextureKey::make(idx));
                }
                return true;
            });
        }

        if (out.empty()) {
            const std::size_t requestedCount =
                env::readSize("ATRAPACIELOS_STRESS_RENDER_TEXTURE_COUNT", 4u, kMaxTextureIds);
            const std::size_t count = std::min(requestedCount, registryCount);
            out.reserve(count);
            for (std::size_t i = 0; i < count; ++i) {
                out.push_back(core::resources::TextureKey::make(static_cast<std::uint32_t>(i)));
            }
        }

        if (out.empty() && fallbackTexture.valid()) {
            out.push_back(fallbackTexture);
        }

        return out;
    }

} // namespace

namespace game::atrapacielos::dev {

    StressRenderChunkContentProvider::StressRenderChunkContentProvider(
        core::resources::ResourceManager& resources,
        const core::resources::TextureKey fallbackTexture,
        const std::int32_t chunkSizeWorld,
        const std::int32_t windowWidthChunks,
        const std::int32_t windowHeightChunks,
        const std::int32_t hysteresisMarginChunks,
        const float viewportWorldW,
        const float viewportWorldH)
        : mChunkSizeWorld(chunkSizeWorld),
          mWindowWidthChunks(windowWidthChunks),
          mWindowHeightChunks(windowHeightChunks),
          mViewportHintW(viewportWorldW),
          mViewportHintH(viewportWorldH) {

        if (mChunkSizeWorld <= 0 || mWindowWidthChunks <= 0 || mWindowHeightChunks <= 0) {
            LOG_PANIC(core::log::cat::Performance,
                      "Stress render provider: invalid chunk/window sizes (chunk={}, win={}x{}).",
                      mChunkSizeWorld, mWindowWidthChunks, mWindowHeightChunks);
        }

        if (!(mViewportHintW > 0.0f) || !(mViewportHintH > 0.0f)) {
            LOG_PANIC(core::log::cat::Performance,
                      "Stress render provider: invalid viewport hint ({:.0f}x{:.0f}).",
                      mViewportHintW, mViewportHintH);
        }

        // 128K per chunk: covers pathological dense hotspot chunks.
        constexpr std::size_t kMaxPerChunk = 131'072u;

        const StressRenderOptions opts = readStressRenderOptionsFromEnv();
        mTextures = buildTextureListFromEnv(resources, fallbackTexture);
        mRenderMode = opts.renderMode;

        mSeed = opts.seed;
        mTotalCount = opts.totalCount;
        mVisibleCount = opts.visibleCount;
        mHotspotRadiusChunks = opts.hotspotRadiusChunks;
        mOverscanChunks = opts.overscanChunks;
        mZLayers = std::max<std::size_t>(1u, opts.zLayers);
        mConfiguredVisibleDensity = opts.visibleDensity;

        // Pathological stack parameters.
        mPathClusterCount = opts.pathClusterCount;
        mPathEntitiesPerCluster = opts.pathEntitiesPerCluster;
        mPathJitterRadius = opts.pathJitterRadius;

        // ========================================================================================
        // Zone budget: iterate PREDICTED runtime window coordinates.
        //
        // Player starts at world chunk (0,0). Config window origin is (0,0).
        // Streaming system shifts origin to (-margin, -margin) on first update because:
        //   focus(0) < innerMin(0 + margin) → desiredOrigin = focus - margin = -margin.
        //
        // We must iterate from (-margin, -margin) to (-margin + W - 1, -margin + H - 1)
        // so the ring classification (distance from player chunk (0,0)) correctly counts
        // chunks with negative coordinates that fall into the hotspot.
        //
        // Example: W=9, H=9, margin=1 → origin=(-1,-1), range (-1,-1)...(7,7).
        //   hotspot (ring ≤ 1): 9 chunks (correct, not 4 as from (0,0)...(8,8)).
        // ========================================================================================
        const std::int32_t predictedOriginX = -hysteresisMarginChunks;
        const std::int32_t predictedOriginY = -hysteresisMarginChunks;

        const ZoneCounts zones = countActualZones(
            predictedOriginX, predictedOriginY,
            mWindowWidthChunks, mWindowHeightChunks,
            mHotspotRadiusChunks, mOverscanChunks);

        const std::size_t actualHotspotChunks = std::max<std::size_t>(1u, zones.hotspot);
        const std::size_t actualOverscanChunks = zones.overscan;
        const std::size_t actualFarChunks = zones.far;

        // ========================================================================================
        // Compute per-zone density.
        //
        // NOTE: TOTAL_COUNT and VISIBLE_COUNT are TARGETS, not exact guarantees.
        // Chunk-based distribution with ceil/divCeil produces systematic overshoot.
        // This is inherent to the chunked architecture and documented as such.
        // ========================================================================================

        if (opts.visibleDensity > 0u) {
            mHotspotPerChunk = opts.visibleDensity;
        } else {
            const std::size_t visible = std::max<std::size_t>(1u, mVisibleCount);
            mHotspotPerChunk = divCeil(visible, actualHotspotChunks);
        }
        mHotspotPerChunk = std::clamp<std::size_t>(mHotspotPerChunk, 1u, kMaxPerChunk);

        std::size_t allocated =
            checkedMul(mHotspotPerChunk, actualHotspotChunks, "hotspotAlloc");

        // Overscan: fraction of hotspot density, scaled up if needed to approach totalCount.
        mOverscanPerChunk = std::max<std::size_t>(1u, mHotspotPerChunk / 4u);
        if (actualOverscanChunks > 0u && allocated < mTotalCount) {
            const std::size_t remain = mTotalCount - allocated;
            const std::size_t addPerChunk = remain / actualOverscanChunks;
            if (addPerChunk > mOverscanPerChunk) {
                mOverscanPerChunk = std::min<std::size_t>(addPerChunk, mHotspotPerChunk);
            }
            allocated += checkedMul(mOverscanPerChunk, actualOverscanChunks, "overscanAlloc");
        }

        // Far: fill remainder to approach totalCount.
        if (actualFarChunks > 0u && allocated < mTotalCount) {
            const std::size_t remain = mTotalCount - allocated;
            mFarPerChunk = divCeil(remain, actualFarChunks);
            mFarPerChunk = std::clamp<std::size_t>(mFarPerChunk, 1u, mOverscanPerChunk);
        } else {
            mFarPerChunk = 1u;
        }

        mMaxEntitiesPerChunk = std::max({mHotspotPerChunk, mOverscanPerChunk, mFarPerChunk});
        mMaxEntitiesPerChunk = std::clamp<std::size_t>(mMaxEntitiesPerChunk, 1u, kMaxPerChunk);

        // Pathological auto-compute: if cluster params not given, derive from hotspotPerChunk.
        if (mRenderMode == StressRenderMode::Pathological) {
            if (mPathClusterCount == 0u || mPathEntitiesPerCluster == 0u) {
                const auto sqrtN = static_cast<std::size_t>(
                    std::sqrt(static_cast<double>(mHotspotPerChunk)));
                if (mPathClusterCount == 0u) {
                    mPathClusterCount = std::max<std::size_t>(4u, sqrtN);
                }
                if (mPathEntitiesPerCluster == 0u) {
                    mPathEntitiesPerCluster = divCeil(mHotspotPerChunk, mPathClusterCount);
                }
            }
        }

        mEnabled = opts.enabled && !mTextures.empty();

        if (mEnabled) {
            LOG_INFO(core::log::cat::Performance,
                     "Stress render provider enabled: mode={}, total={}, visibleTarget={}, "
                     "hotspotRadius={}, overscan={}, hotPerChunk={}, overscanPerChunk={}, "
                     "farPerChunk={}, maxPerChunk={}, textures={}, zLayers={}, seed={}, "
                     "zones(hot={}, over={}, far={}), predictedOrigin=({},{}), "
                     "viewport={:.0f}x{:.0f}",
                     (mRenderMode == StressRenderMode::Pathological) ? "Pathological" : "GameLike",
                     mTotalCount, mVisibleCount, mHotspotRadiusChunks, mOverscanChunks,
                     mHotspotPerChunk, mOverscanPerChunk, mFarPerChunk,
                     mMaxEntitiesPerChunk, mTextures.size(), mZLayers, mSeed,
                     actualHotspotChunks, actualOverscanChunks, actualFarChunks,
                     predictedOriginX, predictedOriginY,
                     mViewportHintW, mViewportHintH);

            if (mRenderMode == StressRenderMode::Pathological) {
                LOG_INFO(core::log::cat::Performance,
                         "  Pathological: clusters={}, perCluster={}, jitter={:.1f}",
                         mPathClusterCount, mPathEntitiesPerCluster, mPathJitterRadius);
            }
        }
    }

    std::size_t StressRenderChunkContentProvider::maxEntitiesPerChunk() const noexcept {
        return mEnabled ? mMaxEntitiesPerChunk : 0u;
    }

    std::size_t StressRenderChunkContentProvider::chunkSpawnCount(
        const core::spatial::ChunkCoord coord) const noexcept {

        const std::size_t overscanMax = mHotspotRadiusChunks + mOverscanChunks;
        const auto zone = classifyChunk(coord, mHotspotRadiusChunks, overscanMax);

        switch (zone) {
        case ChunkZone::Hotspot:
            return mHotspotPerChunk;
        case ChunkZone::Overscan:
            return mOverscanPerChunk;
        case ChunkZone::Far:
            return mFarPerChunk;
        }
        return mFarPerChunk; // unreachable, suppress warning
    }

    std::size_t
    StressRenderChunkContentProvider::fillChunkContent(const core::spatial::ChunkCoord coord,
                                                       std::span<streaming::ChunkEntityDesc> out) {
        if (!mEnabled || out.empty()) {
            return 0u;
        }

        const std::size_t desiredCount = chunkSpawnCount(coord);
        const std::size_t count = std::min(desiredCount, out.size());
        if (count == 0u) {
            return 0u;
        }

        const std::size_t overscanMax = mHotspotRadiusChunks + mOverscanChunks;
        const auto zone = classifyChunk(coord, mHotspotRadiusChunks, overscanMax);
        const bool usePathological =
            (mRenderMode == StressRenderMode::Pathological && zone == ChunkZone::Hotspot);

        std::size_t filled = 0;
        if (usePathological) {
            filled = fillPathological(coord, out, count);
        } else {
            filled = fillGameLike(coord, out, count);
        }

        // Fisher-Yates shuffle for deterministic texture/z interleaving.
        if (filled > 1u) {
            XorShift64 rng{};
            rng.state = mix64(static_cast<std::uint64_t>(mSeed) ^ hashChunkCoord(coord) ^ 0xFF);
            for (std::size_t i = filled - 1u; i > 0u; --i) {
                const std::size_t j = uniformIndex(rng, i + 1u);
                std::swap(out[i], out[j]);
            }
        }

        return filled;
    }

    // ============================================================================================
    // GameLike: uniform distribution across entire chunk area.
    // Realistic world density. Camera sees ~(viewW/chunkSize)×(viewH/chunkSize) fraction.
    // ============================================================================================
    std::size_t StressRenderChunkContentProvider::fillGameLike(
        const core::spatial::ChunkCoord coord,
        std::span<streaming::ChunkEntityDesc> out,
        const std::size_t count) {

        constexpr float kTargetWorldSize = 8.0f;
        constexpr float kStressZBase = -10'000.0f;
        constexpr int kRectA = 8;
        constexpr int kRectB = 16;

        XorShift64 rng{};
        rng.state = mix64(static_cast<std::uint64_t>(mSeed) ^ hashChunkCoord(coord));

        const float chunkSize = static_cast<float>(mChunkSizeWorld);
        const float maxPos = std::nextafter(chunkSize - kTargetWorldSize, 0.0f);
        if (!(maxPos > 0.0f)) {
            LOG_PANIC(core::log::cat::Performance,
                      "Stress render provider: chunk too small (chunkSizeWorld={}).",
                      mChunkSizeWorld);
        }

        const std::size_t texCount = mTextures.size();

        for (std::size_t i = 0; i < count; ++i) {
            const float x = nextFloat01(rng) * maxPos;
            const float y = nextFloat01(rng) * maxPos;
            const std::size_t zIdx = uniformIndex(rng, mZLayers);
            const std::size_t tIdx = uniformIndex(rng, texCount);
            const int rectSizePx = ((rng.nextU32() & 1u) == 0u) ? kRectA : kRectB;

            streaming::ChunkEntityDesc desc{};
            desc.localPos = core::spatial::WorldPosf{x, y};
            desc.texture = mTextures[tIdx];
            desc.textureRect =
                sf::IntRect(sf::Vector2i{0, 0}, sf::Vector2i{rectSizePx, rectSizePx});
            const float s = kTargetWorldSize / static_cast<float>(rectSizePx);
            desc.scale = {s, s};
            desc.origin = {0.0f, 0.0f};
            desc.zOrder = kStressZBase + static_cast<float>(zIdx);

            out[i] = desc;
        }

        return count;
    }

    // ============================================================================================
    // Pathological: stack-based clusters concentrated in viewport-sized band.
    //
    // Band size = actual runtime viewport (from ViewManager), NOT hardcoded.
    // Band is centered in each chunk to maximize overlap with camera for any hotspot chunk.
    //
    // Design (Civ-like stack model):
    //  - Grid-based cluster center placement avoids aliasing with cell boundaries.
    //  - Jitter prevents identical positions (which would degenerate sort/culling).
    //  - Textures and z-layers mixed within each stack for realistic batch-breaking.
    // ============================================================================================
    std::size_t StressRenderChunkContentProvider::fillPathological(
        const core::spatial::ChunkCoord coord,
        std::span<streaming::ChunkEntityDesc> out,
        const std::size_t count) {

        constexpr float kTargetWorldSize = 8.0f;
        constexpr float kStressZBase = -10'000.0f;
        constexpr int kRectA = 8;
        constexpr int kRectB = 16;

        XorShift64 rng{};
        rng.state = mix64(static_cast<std::uint64_t>(mSeed) ^ hashChunkCoord(coord));

        const float chunkSize = static_cast<float>(mChunkSizeWorld);

        // Viewport-sized band within chunk — from runtime ViewManager, not hardcoded.
        const float bandW = std::min(mViewportHintW, chunkSize - kTargetWorldSize);
        const float bandH = std::min(mViewportHintH, chunkSize - kTargetWorldSize);

        // Center the band in the chunk for maximum camera overlap probability.
        const float bandOffX = std::clamp(
            (chunkSize - bandW) * 0.5f, 0.0f, chunkSize - bandW - kTargetWorldSize);
        const float bandOffY = std::clamp(
            (chunkSize - bandH) * 0.5f, 0.0f, chunkSize - bandH - kTargetWorldSize);

        if (!(bandW > 0.0f) || !(bandH > 0.0f)) {
            return fillGameLike(coord, out, count);
        }

        const std::size_t texCount = mTextures.size();
        const std::size_t clusterCount = std::max<std::size_t>(1u, mPathClusterCount);
        const float jitterR = std::max(0.0f, mPathJitterRadius);

        // Grid-based cluster center distribution.
        const std::size_t gridSide = static_cast<std::size_t>(
            std::ceil(std::sqrt(static_cast<double>(clusterCount))));
        const float cellW = bandW / static_cast<float>(gridSide);
        const float cellH = bandH / static_cast<float>(gridSide);

        std::size_t written = 0;
        std::size_t remaining = count;
        std::size_t clusterIdx = 0;

        for (std::size_t gy = 0; gy < gridSide && remaining > 0; ++gy) {
            for (std::size_t gx = 0; gx < gridSide && remaining > 0; ++gx) {
                if (clusterIdx >= clusterCount) {
                    break;
                }

                // Cluster center: grid cell center.
                const float cx = bandOffX + (static_cast<float>(gx) + 0.5f) * cellW;
                const float cy = bandOffY + (static_cast<float>(gy) + 0.5f) * cellH;

                // Entities per this cluster. Last cluster absorbs remainder.
                const std::size_t perCluster = std::min(
                    remaining,
                    (clusterIdx < clusterCount - 1u)
                        ? mPathEntitiesPerCluster
                        : remaining);

                for (std::size_t i = 0; i < perCluster; ++i) {
                    const float jx = nextFloatSigned(rng) * jitterR;
                    const float jy = nextFloatSigned(rng) * jitterR;

                    const float x = std::clamp(cx + jx, 0.0f, chunkSize - kTargetWorldSize);
                    const float y = std::clamp(cy + jy, 0.0f, chunkSize - kTargetWorldSize);

                    const std::size_t zIdx = uniformIndex(rng, mZLayers);
                    const std::size_t tIdx = uniformIndex(rng, texCount);
                    const int rectSizePx = ((rng.nextU32() & 1u) == 0u) ? kRectA : kRectB;

                    streaming::ChunkEntityDesc desc{};
                    desc.localPos = core::spatial::WorldPosf{x, y};
                    desc.texture = mTextures[tIdx];
                    desc.textureRect =
                        sf::IntRect(sf::Vector2i{0, 0}, sf::Vector2i{rectSizePx, rectSizePx});
                    const float s = kTargetWorldSize / static_cast<float>(rectSizePx);
                    desc.scale = {s, s};
                    desc.origin = {0.0f, 0.0f};
                    desc.zOrder = kStressZBase + static_cast<float>(zIdx);

                    out[written++] = desc;
                    --remaining;
                }

                ++clusterIdx;
            }
        }

        // If clusters didn't fill all `count` (rounding), fill remainder uniformly in band.
        while (remaining > 0) {
            const float x = bandOffX + nextFloat01(rng) * bandW;
            const float y = bandOffY + nextFloat01(rng) * bandH;

            const std::size_t zIdx = uniformIndex(rng, mZLayers);
            const std::size_t tIdx = uniformIndex(rng, texCount);
            const int rectSizePx = ((rng.nextU32() & 1u) == 0u) ? kRectA : kRectB;

            streaming::ChunkEntityDesc desc{};
            desc.localPos = core::spatial::WorldPosf{
                std::clamp(x, 0.0f, chunkSize - kTargetWorldSize),
                std::clamp(y, 0.0f, chunkSize - kTargetWorldSize)};
            desc.texture = mTextures[tIdx];
            desc.textureRect =
                sf::IntRect(sf::Vector2i{0, 0}, sf::Vector2i{rectSizePx, rectSizePx});
            const float s = kTargetWorldSize / static_cast<float>(rectSizePx);
            desc.scale = {s, s};
            desc.origin = {0.0f, 0.0f};
            desc.zOrder = kStressZBase + static_cast<float>(zIdx);

            out[written++] = desc;
            --remaining;
        }

        return written;
    }

} // namespace game::atrapacielos::dev

#endif