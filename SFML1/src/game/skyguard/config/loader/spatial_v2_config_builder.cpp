#include "pch.h"

#include "game/skyguard/config/loader/spatial_v2_config_builder.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>

#include "core/log/log_macros.h"

namespace {
    [[nodiscard]] std::string_view overflowPolicyName(core::spatial::OverflowPolicy policy) noexcept {
        switch (policy) {
            case core::spatial::OverflowPolicy::FailFast:
                return "FailFast";
            case core::spatial::OverflowPolicy::Truncate:
                return "Truncate";
            default:
                return "Unknown";
        }
    }
} // namespace

namespace game::skyguard::config {

    core::ecs::SpatialIndexSystemConfig buildSpatialIndexV2ConfigSkyGuard(
        const core::config::EngineSettings& settings,
        const sf::Vector2f worldLogicalSize,
        const sf::Vector2u windowPixelSize) {
        constexpr std::int32_t kChunkSizeWorld = 4096;
        constexpr std::uint32_t kExpectedMaxEntities = 2'000'000;
        constexpr std::size_t kMaxVisibleSprites = 50'000;
        constexpr std::size_t kMaxDirtyEntities = 50'000;
        constexpr std::int32_t kWindowMarginChunks = 1;

        const float cellSizeFloat = settings.spatialCellSize;
        const auto cellSizeWorld = static_cast<std::int32_t>(cellSizeFloat);
        if (cellSizeWorld <= 0 || static_cast<float>(cellSizeWorld) != cellSizeFloat) {
            LOG_PANIC(core::log::cat::ECS,
                      "SpatialIndexV2: spatialCellSize must be a positive integer (value={})",
                      cellSizeFloat);
        }
        if (kChunkSizeWorld % cellSizeWorld != 0) {
            LOG_PANIC(core::log::cat::ECS,
                      "SpatialIndexV2: chunkSizeWorld must be divisible by cellSizeWorld "
                      "(chunkSizeWorld={}, cellSizeWorld={})",
                      kChunkSizeWorld, cellSizeWorld);
        }

        if (!(worldLogicalSize.x > 0.f) || !(worldLogicalSize.y > 0.f)) {
            LOG_PANIC(core::log::cat::ECS,
                      "SpatialIndexV2: worldLogicalSize must be positive ({}x{})",
                      worldLogicalSize.x, worldLogicalSize.y);
        }

        const auto visibleChunksX = static_cast<std::int32_t>(
            std::ceil(static_cast<double>(worldLogicalSize.x) / kChunkSizeWorld));
        const auto visibleChunksY = static_cast<std::int32_t>(
            std::ceil(static_cast<double>(worldLogicalSize.y) / kChunkSizeWorld));

        if (visibleChunksX <= 0 || visibleChunksY <= 0) {
            LOG_PANIC(core::log::cat::ECS,
                      "SpatialIndexV2: computed chunk grid is invalid ({}x{})",
                      visibleChunksX, visibleChunksY);
        }

        const std::int32_t windowWidth = visibleChunksX + (kWindowMarginChunks * 2);
        const std::int32_t windowHeight = visibleChunksY + (kWindowMarginChunks * 2);

        if (windowWidth <= 0 || windowHeight <= 0) {
            LOG_PANIC(core::log::cat::ECS,
                      "SpatialIndexV2: sliding window dimensions invalid ({}x{})",
                      windowWidth, windowHeight);
        }

        const std::uint32_t windowChunks =
            static_cast<std::uint32_t>(windowWidth) * static_cast<std::uint32_t>(windowHeight);
        const std::uint32_t maxResidentChunks = windowChunks;

        const auto overflowMaxNodes = static_cast<std::uint32_t>(
            std::max<std::size_t>(1u, kMaxVisibleSprites / core::spatial::Cell::kInlineCapacity));

        const std::uint32_t shiftBudget = static_cast<std::uint32_t>(
            std::max<std::int32_t>(1, windowWidth + windowHeight - 1));

        core::ecs::SpatialIndexSystemConfig cfg{};
        cfg.index.chunkSizeWorld = kChunkSizeWorld;
        cfg.index.cellSizeWorld = cellSizeWorld;
        cfg.index.maxEntityId = kExpectedMaxEntities;
        cfg.index.marksCapacity = kExpectedMaxEntities;
        cfg.index.overflowPolicy = core::spatial::OverflowPolicy::FailFast;
        cfg.index.overflow = core::spatial::OverflowConfig{
            .nodeCapacity = 32u,
            .maxNodes = overflowMaxNodes};

        cfg.storage.origin = core::spatial::ChunkCoord{0, 0};
        cfg.storage.width = windowWidth;
        cfg.storage.height = windowHeight;
        cfg.storage.maxResidentChunks = maxResidentChunks;

        cfg.maxEntityId = kExpectedMaxEntities;
        cfg.maxDirtyEntities = kMaxDirtyEntities;
        cfg.maxVisibleSprites = kMaxVisibleSprites;
        cfg.maxLoadsPerFrame = shiftBudget;
        cfg.maxUnloadsPerFrame = shiftBudget;
        cfg.hysteresisMarginChunks = kWindowMarginChunks;
        cfg.determinismEnabled = false;

        if (cfg.storage.width <= 0 || cfg.storage.height <= 0) {
            LOG_PANIC(core::log::cat::ECS,
                      "SpatialIndexV2: sliding window dimensions invalid ({}x{})",
                      cfg.storage.width, cfg.storage.height);
        }
        if (cfg.storage.maxResidentChunks <
            static_cast<std::uint32_t>(cfg.storage.width * cfg.storage.height)) {
            LOG_PANIC(core::log::cat::ECS,
                      "SpatialIndexV2: maxResidentChunks too small (maxResident={}, window={}x{})",
                      cfg.storage.maxResidentChunks, cfg.storage.width, cfg.storage.height);
        }
        if (cfg.index.maxEntityId != cfg.maxEntityId) {
            LOG_PANIC(core::log::cat::ECS,
                      "SpatialIndexV2: maxEntityId mismatch (index={}, system={})",
                      cfg.index.maxEntityId, cfg.maxEntityId);
        }

        LOG_INFO(core::log::cat::ECS,
                 "SpatialIndexV2(SkyGuard): window {}x{} px, window {}x{} chunks, "
                 "maxResidentChunks={}, budgets(load={}, unload={}), overflow={} "
                 "(nodes={}, capacity={})",
                 windowPixelSize.x, windowPixelSize.y,
                 cfg.storage.width, cfg.storage.height,
                 cfg.storage.maxResidentChunks,
                 cfg.maxLoadsPerFrame, cfg.maxUnloadsPerFrame,
                 overflowPolicyName(cfg.index.overflowPolicy),
                 cfg.index.overflow.maxNodes, cfg.index.overflow.nodeCapacity);

        return cfg;
    }

} // namespace game::skyguard::config