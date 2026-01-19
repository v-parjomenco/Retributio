#include "pch.h"

#include "game/skyguard/dev/stress_scene.h"

#if !defined(NDEBUG) || defined(SFML1_PROFILE)

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>
#include <system_error>
#include <vector>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include "core/ecs/components/sprite_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/world.h"
#include "core/log/log_macros.h"
#include "core/resources/resource_manager.h"

namespace {

    // --------------------------------------------------------------------------------------------
    // Env helpers
    // --------------------------------------------------------------------------------------------

    template <std::size_t N>
    [[nodiscard]] std::string_view readEnvStringView(const char* name, std::array<char, N>& buf) noexcept {
        static_assert(N > 1, "Buffer must have room for null terminator.");

    #ifdef _WIN32
        std::size_t required = 0;
        if (::getenv_s(&required, buf.data(), buf.size(), name) != 0) {
            return {};
        }
        if (required == 0 || required > buf.size()) {
            return {};
        }
        // required includes the terminating null.
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

    // Visitor: bool(std::string_view token) -> continue?
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
    // Texture list
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

        // 1) Optional explicit list (comma-separated indices).
        // Индексы соответствуют TextureKey.index() (RuntimeKey32 index) в текущем реестре.
        std::array<char, 1024> idsBuf{};
        const std::string_view ids = readEnvStringView("SKYGUARD_STRESS_TEXTURE_IDS", idsBuf);

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

        // 2) Count-based fallback (first N indices).
        if (out.empty()) {
            const std::size_t requestedCount =
                readEnvSize("SKYGUARD_STRESS_TEXTURE_COUNT", /*default*/ 1, /*max*/ kMaxTextureIds);

            const std::size_t count = std::min(requestedCount, registryCount);
            out.reserve(count);
            for (std::size_t i = 0; i < count; ++i) {
                out.push_back(core::resources::TextureKey::make(static_cast<std::uint32_t>(i)));
            }
        }

        // 3) Final fallback
        if (out.empty()) {
            if (fallbackTexture.valid()) {
                out.push_back(fallbackTexture);
            } else {
                LOG_WARN(core::log::cat::Performance,
                         "Stress scene disabled: fallback TextureKey is invalid.");
            }
        }

        return out;
    }

    // --------------------------------------------------------------------------------------------
    // Deterministic RNG + shuffle
    // --------------------------------------------------------------------------------------------

    // Простая, быстрая, детерминированная мешалка (xorshift32).
    struct XorShift32 {
        std::uint32_t state = 0xA3C59AC3u;

        [[nodiscard]] std::uint32_t next() noexcept {
            std::uint32_t x = state;
            x ^= (x << 13);
            x ^= (x >> 17);
            x ^= (x << 5);
            state = x;
            return x;
        }
    };

    [[nodiscard]] std::size_t uniformIndex(XorShift32& rng, const std::size_t bound) noexcept {
        // Bound <= 1'000'000 here. Use mul-high reduction to avoid modulo bias.
        const std::uint64_t x = static_cast<std::uint64_t>(rng.next());
        return static_cast<std::size_t>((x * static_cast<std::uint64_t>(bound)) >> 32);
    }

    // --------------------------------------------------------------------------------------------
    // Spawn
    // --------------------------------------------------------------------------------------------

    void spawnStressSprites(core::ecs::World& world,
                            const std::size_t count,
                            const std::size_t zLayers,
                            const std::vector<core::resources::TextureKey>& textures) {
        if (count == 0 || zLayers == 0 || textures.empty()) {
            return;
        }

        // ----------------------------------------------------------------------------------------
        // Вместо хранения SpawnDesc на N элементов:
        //  - генерируем индексы [0..count) и детерминированно перемешиваем;
        //  - параметры спавна вычисляем из перемешанного индекса.
        //
        // Это эквивалентно "сгенерил desc по i -> перемешал desc",
        // но заметно меньше по памяти и по writes (особенно на 1e6).
        // ----------------------------------------------------------------------------------------

        std::vector<std::uint32_t> order;
        order.resize(count);

        for (std::size_t i = 0; i < count; ++i) {
            order[i] = static_cast<std::uint32_t>(i);
        }

        XorShift32 rng{};
        for (std::size_t i = order.size(); i > 1; --i) {
            const std::size_t j = uniformIndex(rng, i);
            std::swap(order[i - 1], order[j]);
        }

        std::size_t rowLen = static_cast<std::size_t>(std::sqrt(static_cast<double>(count)));
        if (rowLen * rowLen < count) {
            ++rowLen;
        }

        // Оставляем верхний левый угол чище для DebugOverlay.
        const sf::Vector2f start{0.f, 200.f};
        const sf::Vector2f step{10.f, 10.f};

        constexpr float kTargetWorldSize = 8.f;     // хотим ~8x8 в мире, независимо от rect
        constexpr float kStressZBase = -10'000.0f;  // позади любого обычного контента (zOrder >= 0)

        const std::size_t texCount = textures.size();
        constexpr int kRectA = 8;
        constexpr int kRectB = 16;

        for (std::size_t i = 0; i < count; ++i) {
            const core::ecs::Entity e = world.createEntity();
            if (e == core::ecs::NullEntity) {
                LOG_WARN(core::log::cat::Performance,
                         "Stress scene: entity creation failed at i={} (spawned {} of {}).",
                         i,
                         i,
                         count);
                break;
            }

            const std::uint32_t idx = order[i];

            const std::size_t zIdx = static_cast<std::size_t>(idx) % zLayers;
            const std::size_t tIdx = (static_cast<std::size_t>(idx) / zLayers) % texCount;
            const int rectSizePx = ((idx & 1u) == 0u) ? kRectA : kRectB;

            core::ecs::TransformComponent tr{};
            tr.position = {start.x + step.x * static_cast<float>(i % rowLen),
                           start.y + step.y * static_cast<float>(i / rowLen)};
            tr.rotationDegrees = 0.f;

            core::ecs::SpriteComponent sp{};
            sp.texture = textures[tIdx];
            sp.textureRect = sf::IntRect(sf::Vector2i{0, 0}, sf::Vector2i{rectSizePx, rectSizePx});

            const float s = kTargetWorldSize / static_cast<float>(rectSizePx);
            sp.scale = {s, s};
            sp.origin = {0.f, 0.f};
            sp.zOrder = kStressZBase + static_cast<float>(zIdx);

            world.addComponent<core::ecs::TransformComponent>(e, tr);
            world.addComponent<core::ecs::SpriteComponent>(e, sp);
        }
    }

} // namespace

namespace game::skyguard::dev {

    void trySpawnStressSpritesFromEnv(core::ecs::World& world,
                                      core::resources::ResourceManager& resources,
                                      const core::resources::TextureKey fallbackTexture) {
        constexpr std::size_t kMaxStressSprites = 1'000'000;

        const std::size_t stressCount =
            readEnvSize("SKYGUARD_STRESS_SPRITES", /*default*/ 0, /*max*/ kMaxStressSprites);

        if (stressCount == 0) {
            return;
        }

        const std::size_t zLayers =
            readEnvSize("SKYGUARD_STRESS_Z_LAYERS", /*default*/ 5, /*max*/ 256);

        auto textures = buildTextureListFromEnv(resources, fallbackTexture);
        if (textures.empty()) {
            return;
        }

    #if defined(SFML1_PROFILE)
        const char* note = (textures.size() < 2)
            ? " TexSwitches may stay 0: add more via SKYGUARD_STRESS_TEXTURE_IDS or "
              "SKYGUARD_STRESS_TEXTURE_COUNT."
            : "";

        LOG_INFO(core::log::cat::Performance,
                 "Stress scene enabled: spawning {} sprites (zLayers={}, textures={}).{}",
                 stressCount,
                 zLayers,
                 textures.size(),
                 note);
    #endif

        spawnStressSprites(world, stressCount, zLayers, textures);
    }

} // namespace game::skyguard::dev

#else

// Release (без SFML1_PROFILE): намеренно пусто.

#endif
