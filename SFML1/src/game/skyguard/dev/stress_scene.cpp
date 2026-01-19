#include "pch.h"

#include "game/skyguard/dev/stress_scene.h"

#if !defined(NDEBUG) || defined(SFML1_PROFILE)

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <system_error>
#include <vector>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include "core/ecs/components/sprite_component.h"
#include "core/ecs/components/sprite_scaling_data_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/world.h"
#include "core/log/log_macros.h"
#include "core/resources/resource_manager.h"

namespace {

    [[nodiscard]] std::size_t readEnvSize(const char* name,
                                          const std::size_t defaultValue,
                                          const std::size_t maxValue) noexcept {
        const char* s = nullptr;

    #ifdef _WIN32
        std::array<char, 128> buf{};
        std::size_t required = 0;

        if (::getenv_s(&required, buf.data(), buf.size(), name) != 0) {
            return defaultValue;
        }
        if (required == 0 || required > buf.size()) {
            return defaultValue;
        }

        s = buf.data();
    #else
        s = std::getenv(name);
        if (s == nullptr || *s == '\0') {
            return defaultValue;
        }
    #endif

        std::uint64_t value = 0;
        const std::size_t len = std::strlen(s);
        const auto [ptr, ec] = std::from_chars(s, s + len, value);
        if (ec != std::errc{} || ptr != (s + len)) {
            return defaultValue;
        }

        if (value > static_cast<std::uint64_t>(maxValue)) {
            return maxValue;
        }

        return static_cast<std::size_t>(value);
    }

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

    [[nodiscard]] std::vector<core::resources::TextureKey> buildTextureListFromEnv(
        core::resources::ResourceManager& resources,
        const core::resources::TextureKey fallbackTexture) {

        std::vector<core::resources::TextureKey> out;

        const std::size_t requestedCount =
            readEnvSize("SKYGUARD_STRESS_TEXTURE_COUNT", /*default*/ 1, /*max*/ 64);
        const std::size_t registryCount = resources.registry().textureCount();
        const std::size_t count = std::min(requestedCount, registryCount);
        out.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            out.push_back(core::resources::TextureKey::make(static_cast<std::uint32_t>(i)));
        }

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

    struct SpawnDesc {
        core::resources::TextureKey texture{};
        float zOrder = 0.f;
        int rectSizePx = 8; // rect != 1x1 гарантированно
    };

    void spawnStressSprites(core::ecs::World& world,
                            const std::size_t count,
                            const std::size_t zLayers,
                            const std::vector<core::resources::TextureKey>& textures) {
        if (count == 0 || zLayers == 0 || textures.empty()) {
            return;
        }

        // ----------------------------------------------------------------------------------------
        // Генерация SpawnDesc:
        //  - гарантируем присутствие комбинаций (z, texture) по кругу;
        //  - затем детерминированно перемешиваем порядок, чтобы creation-order был "грязным"
        //    (иначе CPU-кэш в draw-фазе будет слишком оптимистичным).
        // ----------------------------------------------------------------------------------------

        std::vector<SpawnDesc> descs;
        descs.reserve(count);

        const std::size_t texCount = textures.size();
        const std::size_t rectVariantCount = 2; // 8px и 16px (оба != 1x1)

        for (std::size_t i = 0; i < count; ++i) {
            const std::size_t zIdx = (i % zLayers);
            const std::size_t tIdx = ((i / zLayers) % texCount);
            const std::size_t rIdx = (i % rectVariantCount);

            SpawnDesc d{};
            d.texture = textures[tIdx];
            // Важно для UX: стресс-сетка должна быть "фоном", а не перекрывать реальную сцену.
            // Делаем отрицательную базу, чтобы оказаться позади любого обычного контента (zOrder >= 0).
            constexpr float kStressZBase = -10'000.0f;
            d.zOrder = kStressZBase + static_cast<float>(zIdx);
            d.rectSizePx = (rIdx == 0) ? 8 : 16;

            descs.push_back(d);
        }

        // Fisher-Yates shuffle (deterministic).
        XorShift32 rng{};
        for (std::size_t i = descs.size(); i > 1; --i) {
            const std::size_t j = static_cast<std::size_t>(rng.next()) % i;
            std::swap(descs[i - 1], descs[j]);
        }

        // ----------------------------------------------------------------------------------------
        // Спавн в детерминированную квадратную сетку.
        // Важно: удерживаем итоговый world-size спрайта примерно постоянным,
        // чтобы не упереться в fill-rate и не "сломать" CPU-тест.
        // ----------------------------------------------------------------------------------------

        std::size_t rowLen =
            static_cast<std::size_t>(std::sqrt(static_cast<double>(count)));
        if (rowLen * rowLen < count) {
            ++rowLen;
        }

        // Оставляем верхний левый угол чище для DebugOverlay (читать метрики должно быть удобно).
        const sf::Vector2f start{0.f, 200.f};
        const sf::Vector2f step{10.f, 10.f};

        constexpr float kTargetWorldSize = 8.f; // хотим ~8x8 в мире, независимо от rect

        for (std::size_t i = 0; i < count; ++i) {
            const core::ecs::Entity e = world.createEntity();
            if (e == core::ecs::NullEntity) {
                LOG_WARN(core::log::cat::Performance,
                         "Stress scene: entity creation failed at i={} (spawned {}).",
                         i, i);
                break;
            }

            core::ecs::TransformComponent tr{};
            tr.position = {
                start.x + step.x * static_cast<float>(i % rowLen),
                start.y + step.y * static_cast<float>(i / rowLen)
            };

            const SpawnDesc& d = descs[i];

            core::ecs::SpriteComponent sp{};
            sp.texture = d.texture;

            // rect != 1x1: реалистичнее, чем 1x1, но остаёмся CPU-ориентированными.
            sp.textureRect = sf::IntRect(sf::Vector2i{0, 0},
                             sf::Vector2i{d.rectSizePx, d.rectSizePx});

            const float s = kTargetWorldSize / static_cast<float>(d.rectSizePx);
            sp.scale = {s, s};
            core::ecs::SpriteScalingDataComponent scalingData{};
            scalingData.baseScale = {s, s};

            sp.origin = {0.f, 0.f};
            sp.zOrder = d.zOrder;

            world.addComponent<core::ecs::TransformComponent>(e, tr);
            world.addComponent<core::ecs::SpriteComponent>(e, sp);
            world.addComponent<core::ecs::SpriteScalingDataComponent>(e, scalingData);
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
        const char* note =
            (textures.size() < 2)
                ? " TexSwitches may stay 0: add more via SKYGUARD_STRESS_TEXTURE_COUNT."
                : "";
        LOG_INFO(core::log::cat::Performance,
                 "Stress scene enabled: spawning {} sprites (zLayers={}, textures={}).{}",
                 stressCount, zLayers, textures.size(), note);
#endif

        spawnStressSprites(world, stressCount, zLayers, textures);
    }

} // namespace game::skyguard::dev

#else

// Release (без SFML1_PROFILE): намеренно пусто.

#endif