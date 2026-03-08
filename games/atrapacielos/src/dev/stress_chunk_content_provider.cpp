#include "pch.h"

#include "dev/stress_chunk_content_provider.h"

#if defined(RETRIBUTIO_PROFILE)

#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <string_view>
#include <system_error>
#include <vector>

#include "core/log/log_macros.h"
#include "core/resources/resource_manager.h"

namespace {

    // --------------------------------------------------------------------------------------------
    // Вспомогательные функции для переменных окружения
    // --------------------------------------------------------------------------------------------

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
        // required включает завершающий нуль-терминатор.
        return std::string_view{buf.data(), required - 1};
    #else
        const char* s = std::getenv(name);
        if (s == nullptr || *s == '\0') {
            return {};
        }
        return std::string_view{s};
    #endif
    }

    [[nodiscard]] std::size_t parseSizeOrDefault(std::string_view s,
                                                 const std::size_t defaultValue,
                                                 const std::size_t maxValue) noexcept {
        if (s.empty()) {
            return defaultValue;
        }

        std::uint64_t value = 0;
        const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
        if (ec != std::errc{} || ptr != (s.data() + s.size())) {
            return defaultValue;
        }

        if (value > static_cast<std::uint64_t>(maxValue)) {
            return maxValue;
        }
        return static_cast<std::size_t>(value);
    }

    [[nodiscard]] std::size_t readEnvSize(const char* name,
                                          const std::size_t defaultValue,
                                          const std::size_t maxValue) noexcept {
        std::array<char, 64> buf{};
        const std::string_view s = readEnvStringView(name, buf);
        return parseSizeOrDefault(s, defaultValue, maxValue);
    }

    [[nodiscard]] std::string_view trimAsciiSpacesTabs(std::string_view s) noexcept {
        while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
            s.remove_prefix(1);
        }
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) {
            s.remove_suffix(1);
        }
        return s;
    }

    // Visitor: bool(std::string_view token) → продолжить итерацию?
    template <typename Visitor>
    void parseCsvTokens(std::string_view csv, Visitor&& visitor) noexcept {
        while (!csv.empty()) {
            const std::size_t comma = csv.find(',');
            std::string_view token = (comma == std::string_view::npos) ? csv : csv.substr(0, comma);
            token = trimAsciiSpacesTabs(token);

            if (!token.empty()) {
                if (!visitor(token)) {
                    return;
                }
            }

            if (comma == std::string_view::npos) {
                return;
            }
            csv.remove_prefix(comma + 1);
        }
    }

    [[nodiscard]] bool parseU32(std::string_view s, std::uint32_t& out) noexcept {
        s = trimAsciiSpacesTabs(s);
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

    // --------------------------------------------------------------------------------------------
    // Список текстур
    // --------------------------------------------------------------------------------------------

    [[nodiscard]] std::vector<core::resources::TextureKey> buildTextureListFromEnv(
        core::resources::ResourceManager& resources,
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

        // 1) Явный список (опционально): индексы текстур через запятую.
        // Индексы соответствуют TextureKey.index() (RuntimeKey32 index) в текущем реестре.
        std::array<char, 1024> idsBuf{};
        const std::string_view ids =
            readEnvStringView("ATRAPACIELOS_STRESS_SPATIAL_TEXTURE_IDS", idsBuf);

        if (!ids.empty()) {
            out.reserve(std::min(kMaxTextureIds, registryCount));

            parseCsvTokens(ids, [&](std::string_view token) noexcept -> bool {
                if (out.size() >= kMaxTextureIds) {
                    return false;
                }

                std::uint32_t idx = 0;
                if (parseU32(token, idx) && (static_cast<std::size_t>(idx) < registryCount)) {
                    out.push_back(core::resources::TextureKey::make(idx));
                }
                return true;
            });
        }

        // 2) Fallback по количеству (первые N индексов).
        if (out.empty()) {
            const std::size_t requestedCount = readEnvSize(
                "ATRAPACIELOS_STRESS_SPATIAL_TEXTURE_COUNT", /*default*/ 1, /*max*/ kMaxTextureIds);

            const std::size_t count = std::min(requestedCount, registryCount);
            out.reserve(count);
            for (std::size_t i = 0; i < count; ++i) {
                out.push_back(core::resources::TextureKey::make(static_cast<std::uint32_t>(i)));
            }
        }

        // 3) Финальный fallback
        if (out.empty()) {
            if (fallbackTexture.valid()) {
                out.push_back(fallbackTexture);
            } else {
                LOG_WARN(core::log::cat::Performance,
                         "Stress provider disabled: fallback TextureKey is invalid.");
            }
        }

        return out;
    }

    // --------------------------------------------------------------------------------------------
    // Детерминированный генератор случайных чисел
    // --------------------------------------------------------------------------------------------

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

} // namespace

namespace game::atrapacielos::dev {

    StressChunkContentProvider::StressChunkContentProvider(
        core::resources::ResourceManager& resources,
        const core::resources::TextureKey fallbackTexture,
        const std::int32_t chunkSizeWorld)
        : mChunkSizeWorld(chunkSizeWorld) {

#if !defined(NDEBUG)
        assert(mChunkSizeWorld > 0 &&
               "StressChunkContentProvider: chunkSizeWorld must be > 0");
#else
        if (mChunkSizeWorld <= 0) {
            LOG_PANIC(core::log::cat::Performance,
                      "StressChunkContentProvider: invalid chunkSizeWorld");
        }
#endif

        constexpr std::size_t kMaxEntitiesPerChunk = 8192;
        constexpr std::size_t kDefaultEntitiesPerChunk = 256;

        const std::size_t enabledFlag = readEnvSize(
            "ATRAPACIELOS_STRESS_SPATIAL_ENABLED", /*default*/ 0, /*max*/ 1);

        const std::size_t envEntities = readEnvSize(
            "ATRAPACIELOS_STRESS_SPATIAL_ENTITIES_PER_CHUNK", /*default*/ 0, kMaxEntitiesPerChunk);

        std::size_t entitiesPerChunk = envEntities;

        if (entitiesPerChunk == 0 && enabledFlag > 0) {
            entitiesPerChunk = kDefaultEntitiesPerChunk;
        }

        const std::size_t zLayers = readEnvSize(
            "ATRAPACIELOS_STRESS_SPATIAL_Z_LAYERS", /*default*/ 5, /*max*/ 256);

        const std::size_t seedValue = readEnvSize(
            "ATRAPACIELOS_STRESS_SPATIAL_SEED", /*default*/ 1, /*max*/ 0xFFFFFFFFu);

        mTextures = buildTextureListFromEnv(resources, fallbackTexture);

        mEntitiesPerChunk = entitiesPerChunk;
        mZLayers = std::max<std::size_t>(1, zLayers);
        mSeed = static_cast<std::uint32_t>(seedValue);
        mEnabled = (mEntitiesPerChunk > 0) && !mTextures.empty();

        if (mEnabled) {
            LOG_INFO(core::log::cat::Performance,
                     "Stress provider enabled: entitiesPerChunk={}, "
                     "zLayers={}, textures={}, seed={}",
                     mEntitiesPerChunk,
                     mZLayers,
                     mTextures.size(),
                     mSeed);
        }
    }

    std::size_t StressChunkContentProvider::maxEntitiesPerChunk() const noexcept {
        return mEnabled ? mEntitiesPerChunk : 0u;
    }

    std::size_t StressChunkContentProvider::fillChunkContent(
        const core::spatial::ChunkCoord coord,
        std::span<streaming::ChunkEntityDesc> out) {

        if (!mEnabled || out.empty()) {
            return 0u;
        }

        const std::size_t count = std::min(mEntitiesPerChunk, out.size());
        if (count == 0u) {
            return 0u;
        }

        XorShift64 rng{};
        rng.state = mix64(static_cast<std::uint64_t>(mSeed) ^ hashChunkCoord(coord));

        constexpr float kTargetWorldSize = 8.f;
        constexpr float kStressZBase = -10'000.0f;
        constexpr int kRectA = 8;
        constexpr int kRectB = 16;

        const float chunkSize = static_cast<float>(mChunkSizeWorld);

        // ВАЖНО: генерируем позиции строго внутри чанка, чтобы AABB (pos + worldExtent)
        // не пересекал границу чанка и не требовал соседних (возможно non-loaded) чанков.
        // nextafter гарантирует, что даже при пограничных значениях max будет < chunkSize.
        const float maxPos = std::nextafter(chunkSize - kTargetWorldSize, 0.0f);

#if !defined(NDEBUG)
        assert(maxPos > 0.0f &&
               "StressChunkContentProvider: chunkSizeWorld too small for target size");
#else
        if (!(maxPos > 0.0f)) {
            LOG_PANIC(core::log::cat::Performance,
                      "StressChunkContentProvider: chunkSizeWorld too small (chunkSizeWorld={}, "
                      "target={})",
                      mChunkSizeWorld,
                      kTargetWorldSize);
        }
#endif

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
            desc.origin = {0.f, 0.f};
            desc.zOrder = kStressZBase + static_cast<float>(zIdx);

            out[i] = desc;
        }

        return count;
    }

    void StressChunkContentProvider::onChunkUnloaded(const core::spatial::ChunkCoord) noexcept {
    }

} // namespace game::atrapacielos::dev

#endif
