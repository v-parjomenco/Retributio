#include "pch.h"

#include "game/skyguard/ecs/systems/player_init_system.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <utility>

#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Vector2.hpp>

#include "core/ecs/components/movement_stats_component.h"
#include "core/ecs/components/sprite_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/components/velocity_component.h"
#include "core/ecs/world.h"
#include "core/log/log_macros.h"
#include "core/resources/resource_manager.h"

#include "game/skyguard/ecs/components/aircraft_control_component.h"
#include "game/skyguard/ecs/components/aircraft_control_bindings_component.h"
#include "game/skyguard/ecs/components/player_tag_component.h"

namespace game::skyguard::ecs {

    PlayerInitSystem::PlayerInitSystem(
        core::resources::ResourceManager& resources,
        std::vector<game::skyguard::config::blueprints::PlayerBlueprint> players)
        : mResources(resources)
        , mPlayers(std::move(players))
        , mHasRun(false) {
    }

    void PlayerInitSystem::update(core::ecs::World& world, const float) {
        if (mHasRun) {
            return;
        }

        LOG_DEBUG(core::log::cat::Gameplay, "PlayerInitSystem: one-shot spawn starting");

        if (mPlayers.size() > 2) {
            LOG_PANIC(core::log::cat::Gameplay,
                      "PlayerInitSystem: too many player blueprints ({}). "
                      "SkyGuard supports max 2 players.",
                      mPlayers.size());
        }

        if (mPlayers.empty()) {
            LOG_WARN(core::log::cat::Gameplay,
                     "PlayerInitSystem: no player blueprints provided (nothing to spawn)");
            mHasRun = true;
            return;
        }

        const std::size_t spawnedCount = mPlayers.size();

        for (std::size_t i = 0; i < mPlayers.size(); ++i) {
            const auto& cfg = mPlayers[i];

            const core::ecs::Entity entity = world.createEntity();

#if !defined(NDEBUG) && defined(SFML1_VERBOSE_PLAYER_SPAWN_LOGS)
            LOG_TRACE(core::log::cat::Gameplay,
                      "PlayerInitSystem: spawning entity {} (index {})",
                      core::ecs::toUint(entity), i);
#endif

            const auto& textureResource = mResources.getTexture(cfg.sprite.texture);
            const sf::Texture& texture = textureResource.get();
            const sf::Vector2u textureSize = texture.getSize();

            const sf::Vector2f origin{static_cast<float>(textureSize.x) * 0.5f,
                                      static_cast<float>(textureSize.y) * 0.5f};

            // HOT COMPONENT: SpriteComponent (читает RenderSystem каждый кадр)
            core::ecs::SpriteComponent spriteComp{};
            spriteComp.texture = cfg.sprite.texture;
            spriteComp.textureRect = sf::IntRect(
                sf::Vector2i{0, 0},
                sf::Vector2i{static_cast<int>(textureSize.x), static_cast<int>(textureSize.y)});

            // Масштаб задаётся конфигом и для SkyGuard считается константой;
            // изменение окна обрабатывается camera/view (letterbox/fit), без ScalingSystem.
            spriteComp.scale = cfg.sprite.scale;
            spriteComp.origin = origin;
            spriteComp.zOrder = 0.f;

            core::ecs::TransformComponent tr{};
            tr.position = cfg.startPosition;
            tr.rotationDegrees = cfg.aircraftControl.initialRotationDegrees;

            core::ecs::VelocityComponent vel{};
            vel.linear = {0.f, 0.f};
            vel.angularDegreesPerSec = 0.f;

            world.addComponent<core::ecs::SpriteComponent>(entity, spriteComp);
            world.addComponent<core::ecs::TransformComponent>(entity, tr);
            world.addComponent<core::ecs::VelocityComponent>(entity, vel);
            world.addComponent<game::skyguard::ecs::AircraftControlComponent>(
                entity,
                game::skyguard::ecs::AircraftControlComponent{
                    cfg.aircraftControl.turnRateDegreesPerSec
                });
            world.addComponent<game::skyguard::ecs::PlayerTagComponent>(
                entity,
                game::skyguard::ecs::PlayerTagComponent{
                    static_cast<std::uint8_t>(i)
                });

            if (i == 0) {
                world.addTagComponent<game::skyguard::ecs::LocalPlayerTagComponent>(entity);
            }

            world.addComponent<core::ecs::MovementStatsComponent>(
                entity,
                core::ecs::MovementStatsComponent{
                    cfg.movement.maxSpeed,
                    cfg.movement.acceleration,
                    cfg.movement.friction
                });

            world.addComponent<game::skyguard::ecs::AircraftControlBindingsComponent>(
                entity,
                game::skyguard::ecs::AircraftControlBindingsComponent{
                    cfg.controls.thrustForward,
                    cfg.controls.thrustBackward,
                    cfg.controls.turnLeft,
                    cfg.controls.turnRight
                });

#if !defined(NDEBUG) && defined(SFML1_VERBOSE_PLAYER_SPAWN_LOGS)
            LOG_DEBUG(core::log::cat::Gameplay,
                      "PlayerInitSystem: player entity {} initialized",
                      core::ecs::toUint(entity));
#endif
        }

        LOG_INFO(core::log::cat::Gameplay,
                 "PlayerInitSystem: spawned {} player entities",
                 spawnedCount);

        mHasRun = true;
    }

} // namespace game::skyguard::ecs
