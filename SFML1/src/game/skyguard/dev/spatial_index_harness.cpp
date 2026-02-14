#include "pch.h"

#include "game/skyguard/dev/spatial_index_harness.h"

#if !defined(NDEBUG) || defined(SFML1_PROFILE)

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>
#include <vector>

#include "core/ecs/entity.h"
#include "core/ecs/systems/spatial_index_system.h"
#include "core/ecs/world.h"
#include "core/log/log_macros.h"
#include "core/spatial/spatial_index.h"
#include "core/spatial/spatial_index_v2.h"
#include "game/skyguard/orchestration/frame_orchestrator.h"

namespace {

    constexpr std::string_view kHarnessEnv = "SKYGUARD_SPATIAL_HARNESS";
    constexpr std::string_view kEntityCountEnv = "SKYGUARD_SPATIAL_HARNESS_ENTITIES";
    constexpr std::string_view kQueryCountEnv = "SKYGUARD_SPATIAL_HARNESS_QUERIES";
    constexpr std::string_view kUpdatePassesEnv = "SKYGUARD_SPATIAL_HARNESS_UPDATES";

    template <std::size_t N>
    [[nodiscard]] std::string_view readEnvStringView(const char* name,
                                                     std::array<char, N>& buf) noexcept {
        static_assert(N > 1, "Buffer must have room for null terminator.");

#ifdef _WIN32
        std::size_t required = 0;
        if (::getenv_s(&required, buf.data(), buf.size(), name) != 0) {
            return {};
        }
        if (required == 0 || required > buf.size()) {
            return {};
        }
        return std::string_view{buf.data(), required - 1};
#else
        const char* s = std::getenv(name);
        if (s == nullptr || *s == '\0') {
            return {};
        }
        return std::string_view{s};
#endif
    }

    [[nodiscard]] bool parseU32(std::string_view s, std::uint32_t& out) noexcept {
        if (s.empty()) {
            return false;
        }
        std::uint32_t value = 0;
        const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
        if (ec != std::errc{} || ptr != (s.data() + s.size())) {
            return false;
        }
        out = value;
        return true;
    }

    [[nodiscard]] std::uint32_t readEnvU32OrDefault(const char* name,
                                                    const std::uint32_t defaultValue,
                                                    const std::uint32_t maxValue) noexcept {
        std::array<char, 64> buf{};
        const std::string_view s = readEnvStringView(name, buf);
        std::uint32_t value = 0;
        if (!parseU32(s, value)) {
            return defaultValue;
        }
        if (value > maxValue) {
            return maxValue;
        }
        return value;
    }

    struct XorShift32 {
        std::uint32_t state = 0xC0FFEE11u;

        [[nodiscard]] std::uint32_t next() noexcept {
            std::uint32_t x = state;
            x ^= (x << 13);
            x ^= (x >> 17);
            x ^= (x << 5);
            state = x;
            return x;
        }
    };

    [[nodiscard]] float uniformFloat(XorShift32& rng, const float maxValue) noexcept {
        const std::uint32_t raw = rng.next();
        const float unit = static_cast<float>(raw) / static_cast<float>(UINT32_MAX);
        return unit * maxValue;
    }

    template <typename T>
    [[nodiscard]] double percentile(std::vector<T>& values, const double p) {
        if (values.empty()) {
            return 0.0;
        }
        std::sort(values.begin(), values.end());
        const double idx = (p / 100.0) * static_cast<double>(values.size() - 1u);
        const auto i = static_cast<std::size_t>(std::floor(idx));
        const auto j = static_cast<std::size_t>(std::ceil(idx));
        if (i == j) {
            return static_cast<double>(values[i]);
        }
        const double t = idx - static_cast<double>(i);
        return static_cast<double>(values[i]) * (1.0 - t) + static_cast<double>(values[j]) * t;
    }

    template <typename T>
    [[nodiscard]] double average(const std::vector<T>& values) {
        if (values.empty()) {
            return 0.0;
        }
        long double sum = 0.0;
        for (const auto& v : values) {
            sum += static_cast<long double>(v);
        }
        return static_cast<double>(sum / static_cast<long double>(values.size()));
    }

    struct HarnessTimings {
        std::vector<std::uint64_t> v1QueryUs{};
        std::vector<std::uint64_t> v2QueryUs{};
        std::vector<std::uint64_t> v1UpdateUs{};
        std::vector<std::uint64_t> v2UpdateUs{};
        std::vector<std::uint32_t> candidateCounts{};
        std::vector<double> overflowCellHitPct{};
        std::vector<std::uint32_t> overflowEntitiesVisited{};
    };

    class QueryDuringUpdateSystem final : public core::ecs::ISystem {
      public:
        explicit QueryDuringUpdateSystem(core::ecs::SpatialIndexSystem* spatialIndexSystem) noexcept
            : mSpatialIndexSystem(spatialIndexSystem) {
        }

        void update(core::ecs::World&, float) override {
            std::array<core::spatial::EntityId32, 16> out{};
            const core::spatial::Aabb2 area{
                0.0f, 0.0f, static_cast<float>(mSpatialIndexSystem->index().chunkSizeWorld()),
                static_cast<float>(mSpatialIndexSystem->index().chunkSizeWorld())};
            (void) mSpatialIndexSystem->index().queryFast(
                area, std::span<core::spatial::EntityId32>{out});
            mQueryExecuted = true;
        }

        void render(core::ecs::World&, sf::RenderWindow&) override {
        }

        [[nodiscard]] bool queryExecuted() const noexcept {
            return mQueryExecuted;
        }

      private:
        core::ecs::SpatialIndexSystem* mSpatialIndexSystem{nullptr};
        bool mQueryExecuted{false};
    };

    [[nodiscard]] std::int64_t toI64(const std::size_t v) noexcept {
        // Для harness значения всегда малы и безопасно умещаются в int64.
        return static_cast<std::int64_t>(v);
    }

    void
    runMarksMaintenanceBeforeUpdateScenario(const core::ecs::SpatialIndexSystemConfig& spatialCfg) {
        core::ecs::World::CreateInfo worldInfo{};
        worldInfo.reserveEntities = spatialCfg.maxEntityId;
        core::ecs::World world{worldInfo};

        auto& spatialSystem = world.addSystem<core::ecs::SpatialIndexSystem>(spatialCfg);

        // Симулируем состояние "конец кадра": marks требуют обслуживания до следующего update.
        spatialSystem.index().debugForceMarksMaintenanceRequired();
        if (!spatialSystem.index().marksClearRequired()) {
            LOG_PANIC(core::log::cat::ECS,
                      "Spatial harness: failed to force marksClearRequired state");
        }

        auto& querySystem = world.addSystem<QueryDuringUpdateSystem>(&spatialSystem);

        game::skyguard::orchestration::FrameOrchestrator orchestrator{};
        orchestrator.bindSpatialIndexSystem(&spatialSystem);
        orchestrator.beginFrameRead();
        world.update(1.0f / 60.0f);

        if (!querySystem.queryExecuted()) {
            LOG_PANIC(
                core::log::cat::ECS,
                "Spatial harness: MarksMaintenanceBeforeUpdate failed (query system not executed)");
        }

        if (spatialSystem.index().marksClearRequired()) {
            LOG_PANIC(core::log::cat::ECS, "Spatial harness: MarksMaintenanceBeforeUpdate failed "
                                           "(maintenance still required)");
        }

        LOG_INFO(core::log::cat::Performance,
                 "Spatial harness scenario passed: MarksMaintenanceBeforeUpdate");
    }

} // namespace

namespace game::skyguard::dev {

    void tryRunSpatialIndexHarnessFromEnv(const core::ecs::SpatialIndexSystemConfig& spatialCfg) {
        std::array<char, 8> enableBuf{};
        const std::string_view enabled = readEnvStringView(kHarnessEnv.data(), enableBuf);
        if (enabled.empty() || enabled == "0") {
            return;
        }

        const std::uint32_t entityCount = readEnvU32OrDefault(kEntityCountEnv.data(),
                                                              /*default*/ 25000u,
                                                              spatialCfg.maxEntityId);
        const std::uint32_t queryCount = readEnvU32OrDefault(kQueryCountEnv.data(),
                                                             /*default*/ 128u,
                                                             10000u);
        const std::uint32_t updatePasses = readEnvU32OrDefault(kUpdatePassesEnv.data(),
                                                              /*default*/ 8u,
                                                              1000u);

        const auto chunkSize = spatialCfg.index.chunkSizeWorld;
        const auto worldWidth = static_cast<float>(spatialCfg.storage.width * chunkSize);
        const auto worldHeight = static_cast<float>(spatialCfg.storage.height * chunkSize);

        if (entityCount == 0 || queryCount == 0 || worldWidth <= 0.f || worldHeight <= 0.f) {
            LOG_WARN(core::log::cat::ECS,
                     "Spatial harness skipped: invalid counts (entities={}, world={}x{})",
                     entityCount, worldWidth, worldHeight);
            return;
        }

        core::spatial::SpatialIndex v1Index{static_cast<float>(spatialCfg.index.cellSizeWorld)};
        core::spatial::SpatialIndexV2Sliding v2Index{};
        v2Index.init(spatialCfg.index, spatialCfg.storage);

        for (std::int32_t y = spatialCfg.storage.origin.y;
             y < spatialCfg.storage.origin.y + spatialCfg.storage.height; ++y) {
            for (std::int32_t x = spatialCfg.storage.origin.x;
                 x < spatialCfg.storage.origin.x + spatialCfg.storage.width; ++x) {
                const bool loaded = v2Index.setChunkState({x, y},
                                                          core::spatial::ResidencyState::Loaded);
                if (!loaded) {
                    LOG_PANIC(core::log::cat::ECS,
                              "Spatial harness: failed to set chunk loaded ({}, {})", x, y);
                }
            }
        }

        std::vector<core::spatial::Aabb2> bounds(static_cast<std::size_t>(entityCount) + 1u);
        std::vector<std::uint32_t> v1Handles(static_cast<std::size_t>(entityCount) + 1u, 0u);

        XorShift32 rng{};
        for (std::uint32_t id = 1; id <= entityCount; ++id) {
            const float w = std::max(8.f, uniformFloat(rng, 256.f));
            const float h = std::max(8.f, uniformFloat(rng, 256.f));
            const float x = uniformFloat(rng, worldWidth - w);
            const float y = uniformFloat(rng, worldHeight - h);
            bounds[id] = core::spatial::Aabb2{x, y, x + w, y + h};

            const core::ecs::Entity entity =
                static_cast<core::ecs::Entity>(static_cast<std::uint32_t>(id));
            v1Handles[id] = v1Index.registerEntity(entity, bounds[id]);

            const auto result = v2Index.registerEntity(id, bounds[id]);
            if (result == core::spatial::WriteResult::Rejected) {
                LOG_PANIC(core::log::cat::ECS,
                          "Spatial harness: V2 registerEntity rejected (id={})", id);
            }
        }

        std::vector<core::ecs::Entity> v1Out;
        std::vector<core::spatial::EntityId32> v2Out(static_cast<std::size_t>(entityCount), 0u);
        v1Out.reserve(entityCount);

        HarnessTimings timings{};
        timings.v1QueryUs.reserve(queryCount);
        timings.v2QueryUs.reserve(queryCount);
        timings.candidateCounts.reserve(queryCount);
        timings.overflowCellHitPct.reserve(queryCount);
        timings.overflowEntitiesVisited.reserve(queryCount);

        std::vector<core::spatial::Aabb2> queries;
        queries.reserve(queryCount);
        for (std::uint32_t i = 0; i < queryCount; ++i) {
            const float w = std::max(64.f, uniformFloat(rng, worldWidth * 0.25f));
            const float h = std::max(64.f, uniformFloat(rng, worldHeight * 0.25f));
            const float x = uniformFloat(rng, worldWidth - w);
            const float y = uniformFloat(rng, worldHeight - h);
            queries.push_back(core::spatial::Aabb2{x, y, x + w, y + h});
        }

        for (const auto& query : queries) {
            v1Out.clear();
            const auto v1Start = std::chrono::steady_clock::now();
            v1Index.query(query, v1Out);
            const auto v1End = std::chrono::steady_clock::now();

            if (v2Index.marksClearRequired()) {
                v2Index.clearMarksTable();
            }

            const auto v2Start = std::chrono::steady_clock::now();
            const std::size_t v2Count = v2Index.queryFast(
                query, std::span<core::spatial::EntityId32>{v2Out.data(), v2Out.size()});
            const auto v2End = std::chrono::steady_clock::now();

            const auto v1Us = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(v1End - v1Start).count());
            const auto v2Us = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(v2End - v2Start).count());
            timings.v1QueryUs.push_back(v1Us);
            timings.v2QueryUs.push_back(v2Us);

            timings.candidateCounts.push_back(static_cast<std::uint32_t>(v2Count));

            const auto stats = v2Index.debugStats();
            const auto cellVisits = stats.lastQuery.cellVisits;
            const auto overflowCells = stats.lastQuery.overflowCellVisits;
            const double pct = (cellVisits == 0)
                                   ? 0.0
                                   : (100.0 * static_cast<double>(overflowCells) /
                                      static_cast<double>(cellVisits));
            timings.overflowCellHitPct.push_back(pct);
            timings.overflowEntitiesVisited.push_back(stats.lastQuery.overflowEntitiesVisited);

            std::vector<std::uint32_t> v1Ids;
            v1Ids.reserve(v1Out.size());
            for (const auto e : v1Out) {
                v1Ids.push_back(static_cast<std::uint32_t>(entt::to_integral(e)));
            }
            const auto v2IdsEnd = v2Out.begin() + static_cast<std::ptrdiff_t>(v2Count);
            std::vector<std::uint32_t> v2Ids(v2Out.begin(), v2IdsEnd);

            std::sort(v1Ids.begin(), v1Ids.end());
            std::sort(v2Ids.begin(), v2Ids.end());
            if (v1Ids != v2Ids) {
                LOG_PANIC(core::log::cat::ECS,
                          "Spatial harness: V1/V2 query mismatch (candidates v1={}, v2={})",
                          toI64(v1Ids.size()), toI64(v2Ids.size()));
            }
        }

        timings.v1UpdateUs.reserve(updatePasses);
        timings.v2UpdateUs.reserve(updatePasses);
        for (std::uint32_t pass = 0; pass < updatePasses; ++pass) {
            const float dx = static_cast<float>((pass + 1u) % 5u);
            const float dy = static_cast<float>((pass + 2u) % 7u);

            auto v1Start = std::chrono::steady_clock::now();
            for (std::uint32_t id = 1; id <= entityCount; ++id) {
                const auto& oldBounds = bounds[id];
                const float width = oldBounds.maxX - oldBounds.minX;
                const float height = oldBounds.maxY - oldBounds.minY;
                const float maxX = std::max(0.0f, worldWidth - width);
                const float maxY = std::max(0.0f, worldHeight - height);

                float newMinX = oldBounds.minX + dx;
                float newMinY = oldBounds.minY + dy;
                if (maxX > 0.0f) {
                    newMinX = std::fmod(newMinX, maxX);
                    if (newMinX < 0.0f) {
                        newMinX += maxX;
                    }
                } else {
                    newMinX = 0.0f;
                }
                if (maxY > 0.0f) {
                    newMinY = std::fmod(newMinY, maxY);
                    if (newMinY < 0.0f) {
                        newMinY += maxY;
                    }
                } else {
                    newMinY = 0.0f;
                }

                core::spatial::Aabb2 newBounds{newMinX, newMinY, newMinX + width, newMinY + height};
                v1Index.update(v1Handles[id], oldBounds, newBounds);
                bounds[id] = newBounds;
            }
            auto v1End = std::chrono::steady_clock::now();

            auto v2Start = std::chrono::steady_clock::now();
            for (std::uint32_t id = 1; id <= entityCount; ++id) {
                const auto result = v2Index.updateEntity(id, bounds[id]);
                if (result == core::spatial::WriteResult::Rejected) {
                    LOG_PANIC(core::log::cat::ECS,
                              "Spatial harness: V2 updateEntity rejected (id={})", id);
                }
            }
            auto v2End = std::chrono::steady_clock::now();

            const auto v1Us = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(v1End - v1Start).count());
            const auto v2Us = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(v2End - v2Start).count());
            timings.v1UpdateUs.push_back(v1Us);
            timings.v2UpdateUs.push_back(v2Us);
        }

        const auto stats = v2Index.debugStats();
        const auto overflowStats = stats.overflow;

        LOG_INFO(core::log::cat::Performance,
                 "Spatial harness (entities={}, queries={}, updates={}):",
                 entityCount, queryCount, updatePasses);
        LOG_INFO(core::log::cat::Performance,
                 "  V1 query us: avg={:.2f} p95={:.2f} p99={:.2f}",
                 average(timings.v1QueryUs),
                 percentile(timings.v1QueryUs, 95.0),
                 percentile(timings.v1QueryUs, 99.0));
        LOG_INFO(core::log::cat::Performance,
                 "  V2 query us: avg={:.2f} p95={:.2f} p99={:.2f}",
                 average(timings.v2QueryUs),
                 percentile(timings.v2QueryUs, 95.0),
                 percentile(timings.v2QueryUs, 99.0));
        LOG_INFO(core::log::cat::Performance,
                 "  V1 update us: avg={:.2f} p95={:.2f} p99={:.2f}",
                 average(timings.v1UpdateUs),
                 percentile(timings.v1UpdateUs, 95.0),
                 percentile(timings.v1UpdateUs, 99.0));
        LOG_INFO(core::log::cat::Performance,
                 "  V2 update us: avg={:.2f} p95={:.2f} p99={:.2f}",
                 average(timings.v2UpdateUs),
                 percentile(timings.v2UpdateUs, 95.0),
                 percentile(timings.v2UpdateUs, 99.0));
        LOG_INFO(core::log::cat::Performance,
                 "  Candidates per query (V2): avg={:.2f} p95={:.2f} p99={:.2f}",
                 average(timings.candidateCounts),
                 percentile(timings.candidateCounts, 95.0),
                 percentile(timings.candidateCounts, 99.0));
        LOG_INFO(core::log::cat::Performance,
                 "  Overflow cell-hit %%: avg={:.2f} p95={:.2f} p99={:.2f}",
                 average(timings.overflowCellHitPct),
                 percentile(timings.overflowCellHitPct, 95.0),
                 percentile(timings.overflowCellHitPct, 99.0));
        LOG_INFO(core::log::cat::Performance,
                 "  Overflow entities visited: avg={:.2f} p95={:.2f} p99={:.2f}",
                 average(timings.overflowEntitiesVisited),
                 percentile(timings.overflowEntitiesVisited, 95.0),
                 percentile(timings.overflowEntitiesVisited, 99.0));
        LOG_INFO(core::log::cat::Performance,
                 "  Overflow pool pressure: peakUsedNodes={} freeDepth={} nodeCapacity={} "
                 "maxNodes={}",
                 overflowStats.peakUsedNodes,
                 overflowStats.freeDepth,
                 overflowStats.nodeCapacity,
                 overflowStats.maxNodes);
        LOG_INFO(core::log::cat::Performance,
                 "  Worst cell density (max entities in cell)={}",
                 stats.worstCellDensity);

        // Deterministic query order check (same state, same output order).
        {
            if (v2Index.marksClearRequired()) {
                v2Index.clearMarksTable();
            }
            const auto countA = v2Index.queryDeterministic(
                queries.front(),
                std::span<core::spatial::EntityId32>{v2Out.data(), v2Out.size()});
            const auto endA = v2Out.begin() + static_cast<std::ptrdiff_t>(countA);
            std::vector<core::spatial::EntityId32> orderA(v2Out.begin(), endA);

            if (v2Index.marksClearRequired()) {
                v2Index.clearMarksTable();
            }
            const auto countB = v2Index.queryDeterministic(
                queries.front(),
                std::span<core::spatial::EntityId32>{v2Out.data(), v2Out.size()});
            const auto endB = v2Out.begin() + static_cast<std::ptrdiff_t>(countB);
            std::vector<core::spatial::EntityId32> orderB(v2Out.begin(), endB);

            if (orderA != orderB) {
                LOG_PANIC(core::log::cat::ECS,
                          "Spatial harness: deterministic query order mismatch");
            }
        }

        runMarksMaintenanceBeforeUpdateScenario(spatialCfg);

        LOG_INFO(core::log::cat::Performance,
                 "Spatial harness complete. Env vars: {}=1, {} (entities), {} (queries), {} "
                 "(updates).",
                 kHarnessEnv, kEntityCountEnv, kQueryCountEnv, kUpdatePassesEnv);
    }

} // namespace game::skyguard::dev

#endif // !defined(NDEBUG) || defined(SFML1_PROFILE)
