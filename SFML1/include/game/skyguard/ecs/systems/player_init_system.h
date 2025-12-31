// ================================================================================================
// File: game/skyguard/ecs/systems/player_init_system.h
// Purpose: One-shot system converting PlayerBlueprint -> runtime ECS components (ID-based)
// Used by: Game (systems wiring), World/SystemManager
// Related headers: sprite_component.h, resource_manager.h, anchor_utils.h, scaling_behavior.h
// ================================================================================================
#pragma once

#include <cstddef>
#include <utility>
#include <vector>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/View.hpp>
#include <SFML/System/Vector2.hpp>

#include "core/ecs/components/lock_behavior_component.h"
#include "core/ecs/components/movement_stats_component.h"
#include "core/ecs/components/scaling_behavior_component.h"
#include "core/ecs/components/sprite_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/components/velocity_component.h"
#include "core/ecs/system.h"
#include "core/ecs/world.h"
#include "core/log/log_macros.h"
#include "core/resources/resource_manager.h"
#include "core/ui/anchor_utils.h"
#include "core/ui/scaling_behavior.h"

#include "game/skyguard/config/blueprints/player_blueprint.h"
#include "game/skyguard/ecs/components/aircraft_control_component.h"
#include "game/skyguard/ecs/components/aircraft_control_bindings_component.h"

namespace game::skyguard::ecs {

    class PlayerInitSystem final : public core::ecs::ISystem {
      public:
        PlayerInitSystem(core::resources::ResourceManager& resources,
                         const sf::Vector2f baseViewSize,
                         const sf::Vector2f initialViewSize,
                         std::vector<game::skyguard::config::blueprints::PlayerBlueprint> players)
            : mResources(resources)
            , mBaseViewSize(baseViewSize)
            , mInitialViewSize(initialViewSize)
            , mPlayers(std::move(players))
            , mHasRun(false) {

        }

        void update(core::ecs::World& world, float) override {
            if (mHasRun) {
                return;
            }

            LOG_DEBUG(core::log::cat::Gameplay, "PlayerInitSystem: one-shot spawn starting");

            if (mPlayers.empty()) {
                LOG_WARN(core::log::cat::Gameplay,
                         "PlayerInitSystem: no player blueprints provided (nothing to spawn)");
                mHasRun = true;
                return;
            }

            const sf::Vector2f safeInitialSize{
                (mInitialViewSize.x > 0.f) ? mInitialViewSize.x : 1.f,
                (mInitialViewSize.y > 0.f) ? mInitialViewSize.y : 1.f
            };
            const sf::View initialView(
                sf::FloatRect({0.f, 0.f}, {safeInitialSize.x, safeInitialSize.y}));

            // Loop-invariant: одинаковый для всех сущностей при первичной инициализации.
            const float initialUniform =
                core::ui::computeUniformFactor(safeInitialSize, mBaseViewSize);

            // Сейчас спавним ровно по числу blueprint'ов.
            const std::size_t spawnedCount = mPlayers.size();

            for (std::size_t i = 0; i < mPlayers.size(); ++i) {
                const auto& cfg = mPlayers[i];

                const core::ecs::Entity entity = world.createEntity();

#if !defined(NDEBUG) && defined(SFML1_VERBOSE_PLAYER_SPAWN_LOGS)
                LOG_TRACE(core::log::cat::Gameplay,
                          "PlayerInitSystem: spawning entity {} (index {})",
                          core::ecs::toUint(entity), i);
#endif

                const sf::Vector2f effectiveScale{
                    cfg.sprite.scale.x * initialUniform,
                    cfg.sprite.scale.y * initialUniform
                };

                const auto& textureResource = mResources.getTexture(cfg.sprite.textureId);
                const sf::Texture& texture = textureResource.get();
                const sf::Vector2u textureSize = texture.getSize();

                sf::Vector2f origin{0.f, 0.f};
                sf::Vector2f anchoredPos = cfg.anchor.startPosition;

                if (cfg.anchor.anchorType != core::ui::AnchorType::None) {
                    auto [computedOrigin, computedPosition] =
                        core::ui::anchors::computeAnchorData(textureSize,
                                                             effectiveScale,
                                                             cfg.anchor.anchorType,
                                                             initialView);
                    origin = computedOrigin;
                    anchoredPos = computedPosition;
                }

                core::ecs::SpriteComponent spriteComp{};
                spriteComp.textureId = cfg.sprite.textureId;
                spriteComp.textureRect =
                    sf::IntRect(sf::Vector2i{0, 0},
                                sf::Vector2i{static_cast<int>(textureSize.x),
                                             static_cast<int>(textureSize.y)});
                spriteComp.baseScale = cfg.sprite.scale;
                spriteComp.scale = effectiveScale;
                spriteComp.origin = origin;
                spriteComp.zOrder = 0.f;

                core::ecs::ScalingBehaviorComponent scalingComp{};
                scalingComp.kind = cfg.anchor.scalingBehavior;
                scalingComp.baseViewSize = mBaseViewSize;
                scalingComp.lastUniform = initialUniform;

                core::ecs::LockBehaviorComponent lockComp{};
                lockComp.kind = cfg.anchor.lockBehavior;
                lockComp.previousViewSize = safeInitialSize;

                core::ecs::TransformComponent tr{};
                tr.position = anchoredPos;
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
                world.addComponent<core::ecs::ScalingBehaviorComponent>(entity, scalingComp);
                world.addComponent<core::ecs::LockBehaviorComponent>(entity, lockComp);

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

        void render(core::ecs::World&, sf::RenderWindow&) override {
        }

      private:
        core::resources::ResourceManager& mResources;
        sf::Vector2f mBaseViewSize;
        sf::Vector2f mInitialViewSize;

        std::vector<game::skyguard::config::blueprints::PlayerBlueprint> mPlayers;

        bool mHasRun;
    };

} // namespace game::skyguard::ecs