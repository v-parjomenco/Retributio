#include "pch.h"

#include "game/skyguard/bootstrap/scene_bootstrap.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <SFML/Graphics/Texture.hpp>

#include "core/log/log_macros.h"
#include "core/resources/resource_manager.h"

namespace {

    using core::resources::FontKey;
    using core::resources::ResourceManager;
    using core::resources::TextureKey;

    using PlayerBlueprint = game::skyguard::config::blueprints::PlayerBlueprint;

    [[nodiscard]] TextureKey resolveTextureOrPanic(ResourceManager& resources,
                                                  const std::string_view canonicalName,
                                                  const std::string_view where) {
        const TextureKey key = resources.findTexture(canonicalName);
        if (!key.valid()) {
            LOG_PANIC(core::log::cat::Config,
                      "[{}] Missing texture key '{}'.",
                      where, canonicalName);
        }
        return key;
    }

    [[nodiscard]] FontKey resolveFontOrPanic(ResourceManager& resources,
                                            const std::string_view canonicalName,
                                            const std::string_view where) {
        const FontKey key = resources.findFont(canonicalName);
        if (!key.valid()) {
            LOG_PANIC(core::log::cat::Config,
                      "[{}] Missing font key '{}'.",
                      where, canonicalName);
        }
        return key;
    }

    template <typename KeyT, std::size_t N>
    void pushUnique(std::array<KeyT, N>& out, std::size_t& count, const KeyT key,
                    const std::string_view where) {
        if (!key.valid()) {
            LOG_PANIC(core::log::cat::Config,
                      "[{}] Invalid resource key passed to scene bootstrap.",
                      where);
        }

        for (std::size_t i = 0; i < count; ++i) {
            if (out[i] == key) {
                return;
            }
        }

        if (count >= out.size()) {
            LOG_PANIC(core::log::cat::Config,
                      "[{}] Preload key list overflow (capacity={}, attempted={} ).",
                      where, out.size(), count + 1);
        }

        out[count++] = key;
    }

    void resolvePlayerSpriteDerived(ResourceManager& resources,
                                   PlayerBlueprint& player,
                                   const std::string_view where) {
        const auto& texRes = resources.expectTextureResident(player.sprite.texture);
        const sf::Texture& tex = texRes.get();

        const sf::Vector2u sz = tex.getSize();
        if (sz.x == 0u || sz.y == 0u) {
            LOG_PANIC(core::log::cat::Resources,
                      "[{}] Player texture has zero size. texKeyIdx={}",
                      where, player.sprite.texture.index());
        }

        // Политика SkyGuard: textureRect = full texture, origin = center.
        player.resolvedTextureRect = sf::IntRect(
            sf::Vector2i{0, 0},
            sf::Vector2i{static_cast<int>(sz.x), static_cast<int>(sz.y)});

        player.resolvedOrigin = sf::Vector2f{
            static_cast<float>(sz.x) * 0.5f,
            static_cast<float>(sz.y) * 0.5f
        };
    }

} // namespace

namespace game::skyguard::bootstrap {

    SceneBootstrapResult preloadAndResolveInitialScene(ResourceManager& resources,
                                                      SceneBootstrapConfig& config) {
        static constexpr std::string_view kWhere = "SceneBootstrap::preloadAndResolveInitialScene";

        // SkyGuard поддерживает максимум 2 игрока (архитектурное ограничение MVP).
        // Лучше ловить это на границе сцены, до прелоада и спавна.
        if (config.players.size() > 2) {
            LOG_PANIC(core::log::cat::Gameplay,
                      "[{}] Too many player blueprints ({}). SkyGuard supports max 2 players.",
                      kWhere, config.players.size());
        }

        SceneBootstrapResult result{};
        result.defaultFontKey = resolveFontOrPanic(resources, "core.font.default", kWhere);
        result.backgroundKey =
            resolveTextureOrPanic(resources, "skyguard.background.desert", kWhere);

        // Preload list: маленький фиксированный набор (фон + 2 игрока + stress).
        std::array<TextureKey, 4> textures{};
        std::size_t textureCount = 0;

        pushUnique(textures, textureCount, result.backgroundKey, kWhere);

        for (auto& p : config.players) {
            pushUnique(textures, textureCount, p.sprite.texture, kWhere);
        }

#if defined(SFML1_PROFILE)
        // PROFILE: стресс-сцена использует текстуру игрока.
        result.stressPlayerKey =
            resolveTextureOrPanic(resources, "skyguard.sprite.player", kWhere);
        pushUnique(textures, textureCount, *result.stressPlayerKey, kWhere);
#endif

        // Preload сцены (textures/fonts). Sounds в SkyGuard пока не используются.
        resources.preloadTextures(std::span<const TextureKey>(textures.data(), textureCount));
        {
            const std::array<FontKey, 1> fonts{result.defaultFontKey};
            resources.preloadFonts(fonts);
        }

        // Derived поля: вычисляем после preload, resident-only.
        for (auto& p : config.players) {
            resolvePlayerSpriteDerived(resources, p, kWhere);
        }

        return result;
    }

} // namespace game::skyguard::bootstrap
