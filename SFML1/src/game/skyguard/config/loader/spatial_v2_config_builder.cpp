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

    /// ceil(v * num / den) без float.
    /// Вызывается ТОЛЬКО на cold path (config init). Входные диапазоны:
    ///  v   ≤ activeSetCeiling (до ~10M при 32×32 × 8192 stress)
    ///  num ≤ 5, den ≥ 1 → промежуточное v*num ≤ ~50M.
    /// На x64 переполнение size_t невозможно; на x32 — нет, если v ≤ ~800M.
    /// Для безопасности вызывающий код использует checkedMulSizeT() до вызова mulCeil().
    [[nodiscard]] constexpr std::size_t mulCeil(std::size_t v,
                                               std::size_t num,
                                               std::size_t den) noexcept {
        return (v * num + den - 1u) / den;
    }

    [[nodiscard]] constexpr std::size_t divCeil(std::size_t num, std::size_t den) noexcept {
        return (num + den - 1u) / den;
    }

    /// Checked size_t multiplication. Cold path only.
    /// Переполнение size_t реально на x32 (4 GB ceiling) при больших окнах/бюджетах.
    [[nodiscard]] std::size_t checkedMulSizeT(
        const std::size_t a,
        const std::size_t b,
        const std::string_view context) {
        if (a != 0u && b > (std::numeric_limits<std::size_t>::max() / a)) {
            LOG_PANIC(core::log::cat::ECS,
                      "SpatialIndexV2(SkyGuard): {} overflow (a={}, b={})",
                      context, a, b);
        }
        return a * b;
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
        // В SkyGuard текущая "реальность" ≈ peak активного набора,
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

        // Mutable: Profile-стресс может переопределить через ENV
        // (декаплинг окна стриминга от viewport камеры).
        std::int32_t windowWidth = visibleChunksX + (kWindowMarginChunks * 2);
        std::int32_t windowHeight = visibleChunksY + (kWindowMarginChunks * 2);

        if (windowWidth <= 0 || windowHeight <= 0) {
            LOG_PANIC(core::log::cat::ECS,
                      "SpatialIndexV2: sliding window dimensions invalid ({}x{})",
                      windowWidth, windowHeight);
        }

        // Viewport-derived: сколько чанков покрывает камера + margin.
        // Сохраняем ДО возможного ENV-override — нужно для maxVisibleSprites.
        const std::int32_t viewportWindowWidth = windowWidth;
        const std::int32_t viewportWindowHeight = windowHeight;

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

                // ENV-override: декаплинг размеров окна стриминга от viewport камеры.
                // Titan-like сценарий: viewport маленький (1920×1080 → 3×3 chunks),
                // окно стриминга большое (16×16 = 256 chunks → 2M entities).
                // queryFast обходит только viewport-чанки; остальные загружены, но не видимы.
                const auto envW = readEnvU32("SKYGUARD_STRESS_WINDOW_WIDTH");
                const auto envH = readEnvU32("SKYGUARD_STRESS_WINDOW_HEIGHT");

                if (envW && (*envW > 0u)) {
                    windowWidth = static_cast<std::int32_t>(*envW);
                }
                if (envH && (*envH > 0u)) {
                    windowHeight = static_cast<std::int32_t>(*envH);
                }

                if (windowWidth <= 0 || windowHeight <= 0) {
                    LOG_PANIC(core::log::cat::ECS,
                              "SpatialIndexV2(SkyGuard): stress window dimensions invalid "
                              "({}x{}) after ENV override",
                              windowWidth, windowHeight);
                }

                const bool windowOverridden =
                    (windowWidth != viewportWindowWidth ||
                     windowHeight != viewportWindowHeight);

                if (windowOverridden &&
                    (windowWidth < viewportWindowWidth ||
                     windowHeight < viewportWindowHeight)) {
                    LOG_WARN(core::log::cat::ECS,
                             "SpatialIndexV2(SkyGuard): stress window ({}x{}) меньше viewport "
                             "window ({}x{}). Часть видимых чанков может быть non-Loaded.",
                             windowWidth, windowHeight,
                             viewportWindowWidth, viewportWindowHeight);
                }

                // Бюджеты:
                //  - visible (query output) = viewport window (камера + margin).
                //    Vertex buffer RenderSystem масштабируется по этому числу.
                //  - dirty (active set)     = полное окно стриминга.
                //    Определяет maxEntityId, marks, entity records.
                const std::size_t stressWindowChunks = checkedMulSizeT(
                    static_cast<std::size_t>(windowWidth),
                    static_cast<std::size_t>(windowHeight),
                    "stressWindowChunks");
                const std::size_t activeSetCeilingTmp = checkedMulSizeT(
                    stressWindowChunks, maxEntitiesPerChunkCeiling, "activeSetCeilingTmp");

                if (windowOverridden) {
                    const std::size_t viewportChunks = checkedMulSizeT(
                        static_cast<std::size_t>(viewportWindowWidth),
                        static_cast<std::size_t>(viewportWindowHeight),
                        "viewportChunks");
                    maxVisibleSprites = checkedMulSizeT(
                        viewportChunks, maxEntitiesPerChunkCeiling, "maxVisibleSprites(viewport)");
                } else {
                    maxVisibleSprites = activeSetCeilingTmp;
                }
                maxDirtyEntities = activeSetCeilingTmp;
            }
        }
#endif

        // windowChunks вычисляется ПОСЛЕ возможного ENV-override окна стриминга.
        // Один раз, const. Checked: на x32 size_t=uint32, int32×int32 может wraparound.
        const std::size_t windowChunksSizeT = checkedMulSizeT(
            static_cast<std::size_t>(windowWidth),
            static_cast<std::size_t>(windowHeight),
            "windowChunks");

        if (windowChunksSizeT == 0u ||
            windowChunksSizeT > static_cast<std::size_t>(
                std::numeric_limits<std::uint32_t>::max())) {
            LOG_PANIC(core::log::cat::ECS,
                      "SpatialIndexV2: windowChunks overflow/invalid ({}x{})",
                      windowWidth, windowHeight);
        }

        const std::uint32_t windowChunks = static_cast<std::uint32_t>(windowChunksSizeT);
        const std::uint32_t maxResidentChunks = windowChunks;

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

        const std::size_t activeSetCeiling = checkedMulSizeT(
            static_cast<std::size_t>(windowChunks), maxEntitiesPerChunkCeiling, "activeSetCeiling");

        std::size_t nonStreamingHeadroom = 0u;

#if defined(SFML1_PROFILE)
        // Profile stress: fixed large buffer to handle any spike
        if (activeSetCeiling >= 100'000u) {
            nonStreamingHeadroom = 100'000u; // Fixed 100K for mega-stress
        } else {
            nonStreamingHeadroom = activeSetCeiling / 10u; // 10% for smaller scenarios
        }
#else
        // Debug/Release: conservative scaling
        constexpr std::size_t kBaseHeadroom = 2048u;
        const std::size_t scalingHeadroom =
            std::min<std::size_t>(activeSetCeiling / 100u, 16384u);
        nonStreamingHeadroom = kBaseHeadroom + scalingHeadroom;
#endif

        // Checked addition (x32 safety).
        if (activeSetCeiling > (std::numeric_limits<std::size_t>::max() - nonStreamingHeadroom)) {
            LOG_PANIC(core::log::cat::ECS,
                      "SpatialIndexV2(SkyGuard): activeSetCeiling + headroom overflow "
                      "(activeSetCeiling={}, headroom={})",
                      activeSetCeiling, nonStreamingHeadroom);
        }

        const std::size_t adjustedCeiling = activeSetCeiling + nonStreamingHeadroom;

        const std::uint32_t expectedMaxEntities =
            computeExpectedMaxEntitiesSkyGuardFromActiveSet(adjustedCeiling);

        // overflow.maxNodes: uint32 range guard (cold path, zero cost).
        const std::size_t rawOverflowMaxNodes = std::max<std::size_t>(
            1u, maxVisibleSprites / core::spatial::Cell::kInlineCapacity);

        if (rawOverflowMaxNodes >
            static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
            LOG_PANIC(core::log::cat::ECS,
                      "SpatialIndexV2(SkyGuard): overflow.maxNodes {} exceeds uint32 range",
                      rawOverflowMaxNodes);
        }
        const auto overflowMaxNodes = static_cast<std::uint32_t>(rawOverflowMaxNodes);

        // shiftBudget = максимальное количество чанков для load/unload за один сдвиг окна.
        // При diagonal step это windowWidth + windowHeight - 1 (1 колонка + 1 строка минус угол).
        //
        // ОГРАНИЧЕНИЕ: бюджет по ЧАНКАМ, не по СУЩНОСТЯМ.
        // int64: формально windowWidth + windowHeight может превысить INT32_MAX при экстремальных
        // ENV override.
        const std::int64_t shiftBudget64 =
            static_cast<std::int64_t>(windowWidth) +
            static_cast<std::int64_t>(windowHeight) - 1;

        if (shiftBudget64 <= 0 ||
            shiftBudget64 > static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max())) {
            LOG_PANIC(core::log::cat::ECS,
                      "SpatialIndexV2(SkyGuard): shiftBudget {} invalid (window {}x{})",
                      shiftBudget64, windowWidth, windowHeight);
        }

        const auto shiftBudget = static_cast<std::uint32_t>(shiftBudget64);

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
                 "SpatialIndexV2(SkyGuard): display {}x{} px, viewport {}x{} chunks, "
                 "streamWindow {}x{} chunks, "
                 "activeSetCeiling={} (streaming={}, nonStreamingHeadroom={}), "
                 "maxEntitiesPerChunk={}, expectedMaxEntities={}, "
                 "budgets(visible={}, dirty={}), maxResidentChunks={}, budgets(load={}, unload={}),"
                 " overflow(nodes={}, capacity={})",
                 windowPixelSize.x, windowPixelSize.y,
                 viewportWindowWidth, viewportWindowHeight,
                 cfg.storage.width, cfg.storage.height,
                 adjustedCeiling, activeSetCeiling, nonStreamingHeadroom,
                 maxEntitiesPerChunkCeiling,
                 cfg.maxEntityId,
                 cfg.maxVisibleSprites, cfg.maxDirtyEntities,
                 cfg.storage.maxResidentChunks,
                 cfg.maxLoadsPerFrame, cfg.maxUnloadsPerFrame,
                 cfg.index.overflow.maxNodes, cfg.index.overflow.nodeCapacity);

        return cfg;
    }

} // namespace game::skyguard::config