#include "pch.h"

#include "game/skyguard/config/loader/spatial_v2_config_builder.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <system_error>

#if defined(SFML1_PROFILE)
    #include <cstdlib>
#endif

#include "core/log/log_macros.h"

namespace {

#if defined(SFML1_PROFILE)

    [[nodiscard]] std::optional<std::uint64_t> readEnvU64(const char* name) noexcept {
    #if defined(_MSC_VER)
        #pragma warning(push)
        #pragma warning(disable : 4996) // getenv
    #endif
        const char* s = std::getenv(name);
    #if defined(_MSC_VER)
        #pragma warning(pop)
    #endif
        if (s == nullptr || *s == '\0') {
            return std::nullopt;
        }

        std::uint64_t value = 0;
        const char* end = s;
        while (*end != '\0') {
            ++end;
        }

        const auto [ptr, ec] = std::from_chars(s, end, value);
        if (ec != std::errc{} || ptr != end) {
            return std::nullopt;
        }
        return value;
    }

    [[nodiscard]] std::optional<std::uint32_t> readEnvU32(const char* name) noexcept {
        const auto v64 = readEnvU64(name);
        if (!v64.has_value()) {
            return std::nullopt;
        }
        if (*v64 > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
            LOG_PANIC(core::log::cat::ECS,
                      "SpatialIndexV2(SkyGuard): env {}={} exceeds uint32 range",
                      name, *v64);
        }
        return static_cast<std::uint32_t>(*v64);
    }

#endif // defined(SFML1_PROFILE)

    [[nodiscard]] constexpr std::size_t mulCeil(std::size_t v,
                                               std::size_t num,
                                               std::size_t den) noexcept {
        // ceil(v * num / den) без float. Диапазоны проекта малы (десятки миллионов максимум),
        // переполнение size_t здесь не ожидается.
        return (v * num + den - 1u) / den;
    }

    [[nodiscard]] constexpr std::size_t divCeil(std::size_t num, std::size_t den) noexcept {
        return (num + den - 1u) / den;
    }

    [[nodiscard]] std::uint32_t computeExpectedMaxEntitiesSkyGuardFromActiveSet(
        const std::size_t activeSetCeiling) {

        if (activeSetCeiling == 0u) {
            LOG_PANIC(core::log::cat::ECS,
                      "SpatialIndexV2(SkyGuard): invalid active-set ceiling (0)");
        }

        // +1: id==0 зарезервирован.
        const std::size_t withSentinel = activeSetCeiling + 1u;

        // Небольшой headroom под реальный peak (без раздувания памяти).
        const std::size_t reserve = withSentinel / 8u; // 12.5%
        const std::size_t raw = withSentinel + reserve;

#if defined(SFML1_PROFILE)
        constexpr std::size_t kMulNum = 5u;
        constexpr std::size_t kMulDen = 4u; // 1.25x
#elif !defined(NDEBUG)
        constexpr std::size_t kMulNum = 3u;
        constexpr std::size_t kMulDen = 2u; // 1.5x
#else
        constexpr std::size_t kMulNum = 5u;
        constexpr std::size_t kMulDen = 4u; // 1.25x
#endif

        const std::size_t cap = mulCeil(raw, kMulNum, kMulDen);

        if (cap > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
            LOG_PANIC(core::log::cat::ECS,
                      "SpatialIndexV2(SkyGuard): expected maxEntityId {} exceeds uint32 range",
                      cap);
        }

        return static_cast<std::uint32_t>(cap);
    }

} // namespace

namespace game::skyguard::config {

    std::size_t computeStableIdCapacitySkyGuard(
        const core::ecs::SpatialIndexSystemConfig& spatialCfg) noexcept {

        // StableIdService индексируется по raw EnTT index,
        // а назначение происходит в World::createEntity.
        // В SkyGuard текущая “реальность” ≈ peak активного набора,
        // который уже ограничен SpatialIndex maxEntityId.
        std::size_t base = static_cast<std::size_t>(spatialCfg.maxEntityId) + 1u; // +1 sentinel

        const std::size_t churnReserve = base / 16u;
        const std::size_t raw = base + churnReserve;

#if defined(SFML1_PROFILE)
        constexpr std::size_t kMulNum = 2u;
        constexpr std::size_t kMulDen = 1u; // 2.0x (профильный churn/стресс)
#elif !defined(NDEBUG)
        constexpr std::size_t kMulNum = 3u;
        constexpr std::size_t kMulDen = 2u; // 1.5x
#else
        constexpr std::size_t kMulNum = 5u;
        constexpr std::size_t kMulDen = 4u; // 1.25x
#endif
        return mulCeil(raw, kMulNum, kMulDen);
    }

    core::ecs::SpatialIndexSystemConfig buildSpatialIndexV2ConfigSkyGuard(
        const core::config::EngineSettings& settings,
        const sf::Vector2f worldLogicalSize,
        const sf::Vector2u windowPixelSize) {

        constexpr std::int32_t kChunkSizeWorld = 4096;
        constexpr std::int32_t kWindowMarginChunks = 1;

        // Базовые бюджеты SkyGuard (Debug/Release).
        std::size_t maxVisibleSprites = 50'000;
        std::size_t maxDirtyEntities = 50'000;

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

        const std::size_t windowChunksSizeT =
            static_cast<std::size_t>(windowWidth) * static_cast<std::size_t>(windowHeight);

        if (windowChunksSizeT == 0u ||
            windowChunksSizeT > static_cast<std::size_t>(
                std::numeric_limits<std::uint32_t>::max())) {
                    LOG_PANIC(core::log::cat::ECS,
                              "SpatialIndexV2: windowChunks overflow/invalid ({}x{})",
                              windowWidth, windowHeight);
        }

        const std::uint32_t windowChunks = static_cast<std::uint32_t>(windowChunksSizeT);
        const std::uint32_t maxResidentChunks = windowChunks;

        std::size_t maxEntitiesPerChunkCeiling = 0u;

#if defined(SFML1_PROFILE)
        {
            // Семантика строго как у StressChunkContentProvider:
            constexpr std::uint32_t kMaxEntitiesPerChunk = 8192u;
            constexpr std::uint32_t kDefaultEntitiesPerChunk = 256u;

            std::uint32_t perChunk = 0u;

            const auto envPerChunk = readEnvU32("SKYGUARD_STRESS_ENTITIES_PER_CHUNK");

            if (envPerChunk && (*envPerChunk > 0u)) {
                perChunk = std::min(*envPerChunk, kMaxEntitiesPerChunk);
            }

            bool stressEnabled = (perChunk > 0u);
            if (!stressEnabled) {
                if (const auto en = readEnvU32("SKYGUARD_STRESS_ENABLED"); en && (*en > 0u)) {
                    stressEnabled = true;
                }
            }

            if (stressEnabled) {
                if (perChunk == 0u) {
                    perChunk = kDefaultEntitiesPerChunk;
                }

                maxEntitiesPerChunkCeiling = static_cast<std::size_t>(perChunk);

                const std::size_t activeSetCeilingTmp =
                    windowChunksSizeT * maxEntitiesPerChunkCeiling;

                // В стресс-режиме budgets должны соответствовать реальному active set.
                maxVisibleSprites = activeSetCeilingTmp;
                maxDirtyEntities = activeSetCeilingTmp;
            }
        }
#endif

        if (maxEntitiesPerChunkCeiling == 0u) {
            const std::size_t base = std::max(maxVisibleSprites, maxDirtyEntities);
            if (base == 0u) {
                LOG_PANIC(core::log::cat::ECS,
                          "SpatialIndexV2(SkyGuard): invalid budgets (visible=0 and dirty=0)");
            }

            // Ceiling на chunk, выводим из общего бюджета и размера окна.
            maxEntitiesPerChunkCeiling = std::max<std::size_t>(
                1u, divCeil(base, static_cast<std::size_t>(windowChunks)));
        }

        const std::size_t activeSetCeiling =
            static_cast<std::size_t>(windowChunks) * maxEntitiesPerChunkCeiling;

        const std::uint32_t expectedMaxEntities =
            computeExpectedMaxEntitiesSkyGuardFromActiveSet(activeSetCeiling);

        const auto overflowMaxNodes = static_cast<std::uint32_t>(
            std::max<std::size_t>(1u, maxVisibleSprites / core::spatial::Cell::kInlineCapacity));

        const std::uint32_t shiftBudget = static_cast<std::uint32_t>(
            std::max<std::int32_t>(1, windowWidth + windowHeight - 1));

        core::ecs::SpatialIndexSystemConfig cfg{};

        cfg.index.chunkSizeWorld = kChunkSizeWorld;
        cfg.index.cellSizeWorld = cellSizeWorld;

        cfg.index.maxEntityId = expectedMaxEntities;
        cfg.index.marksCapacity = expectedMaxEntities;

        cfg.index.overflow = core::spatial::OverflowConfig{
            .nodeCapacity = 32u,
            .maxNodes = overflowMaxNodes};

        cfg.storage.origin = core::spatial::ChunkCoord{0, 0};
        cfg.storage.width = windowWidth;
        cfg.storage.height = windowHeight;
        cfg.storage.maxResidentChunks = maxResidentChunks;

        cfg.maxEntityId = expectedMaxEntities;
        cfg.maxDirtyEntities = maxDirtyEntities;
        cfg.maxVisibleSprites = maxVisibleSprites;
        cfg.maxLoadsPerFrame = shiftBudget;
        cfg.maxUnloadsPerFrame = shiftBudget;
        cfg.hysteresisMarginChunks = kWindowMarginChunks;
        cfg.determinismEnabled = false;

        if (cfg.index.maxEntityId != cfg.maxEntityId) {
            LOG_PANIC(core::log::cat::ECS,
                      "SpatialIndexV2: maxEntityId mismatch (index={}, system={})",
                      cfg.index.maxEntityId, cfg.maxEntityId);
        }

        LOG_INFO(core::log::cat::ECS,
                 "SpatialIndexV2(SkyGuard): window {}x{} px, window {}x{} chunks, "
                 "activeSetCeiling={} (maxPerChunk={}), expectedMaxEntities={}, "
                 "budgets(visible={}, dirty={}), maxResidentChunks={}, budgets(load={}, unload={}),"
                 " overflow(nodes={}, capacity={})",
                 windowPixelSize.x, windowPixelSize.y,
                 cfg.storage.width, cfg.storage.height,
                 activeSetCeiling, maxEntitiesPerChunkCeiling,
                 cfg.maxEntityId,
                 cfg.maxVisibleSprites, cfg.maxDirtyEntities,
                 cfg.storage.maxResidentChunks,
                 cfg.maxLoadsPerFrame, cfg.maxUnloadsPerFrame,
                 cfg.index.overflow.maxNodes, cfg.index.overflow.nodeCapacity);

        return cfg;
    }

} // namespace game::skyguard::config
