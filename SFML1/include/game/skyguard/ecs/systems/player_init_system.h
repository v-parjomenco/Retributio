// ================================================================================================
// File: game/skyguard/ecs/systems/player_init_system.h (FIXED - правильный lastUniform)
// Purpose: One-shot system converting PlayerBlueprint → runtime ECS components (ID-based)
// Used by: World (systems wiring in Game)
// Related headers: player_config_component.h, sprite_component.h, resource_manager.h
// ================================================================================================
#pragma once

#include <utility>
#include <vector>

#include <SFML/Graphics/RenderWindow.hpp>

#include "core/ecs/components/keyboard_control_component.h"
#include "core/ecs/components/lock_behavior_component.h"
#include "core/ecs/components/movement_stats_component.h"
#include "core/ecs/components/scaling_behavior_component.h"
#include "core/ecs/components/sprite_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/components/velocity_component.h"
#include "core/ecs/system.h"
#include "core/ecs/world.h"
#include "core/resources/resource_manager.h"
#include "core/ui/anchor_utils.h"
#include "core/ui/scaling_behavior.h" // ← для computeUniformFactor

#include "game/skyguard/ecs/components/player_config_component.h"

namespace game::skyguard::ecs {

    class PlayerInitSystem final : public core::ecs::ISystem {
      public:
        explicit PlayerInitSystem(core::resources::ResourceManager& res, sf::Vector2f baseViewSize)
            : mResources(res), mBaseViewSize(baseViewSize), mHasRun(false) {
        }

        void update(core::ecs::World& world, float) override {
            if (mHasRun) {
                return;
            }

            auto view = world.view<PlayerConfigComponent>();

            LOG_DEBUG(core::log::cat::Gameplay,
                      "PlayerInitSystem: one-shot initialization starting");

            std::vector<core::ecs::Entity> toRemove;
            int processedCount = 0;

            for (auto [entity, cfgComp] : view.each()) {
                ++processedCount;

                LOG_TRACE(core::log::cat::Gameplay, "Initializing player entity {} (iteration {})",
                          core::ecs::toUint(entity), processedCount);

                const auto& cfg = cfgComp.config;

                // Получаем текстуру для вычисления размеров (для anchor'а)
                const auto& textureResource = mResources.getTexture(cfg.sprite.textureId);
                const sf::Texture& texture = textureResource.get();
                const sf::Vector2u textureSize = texture.getSize();

                // Вычисляем anchor данные
                sf::View defaultView(sf::FloatRect({0.f, 0.f}, {mBaseViewSize.x, mBaseViewSize.y}));

                sf::Vector2f origin{0.f, 0.f};
                sf::Vector2f anchoredPos = cfg.anchor.startPosition;

                if (cfg.anchor.anchorType != core::ui::AnchorType::None) {
                    auto [computedOrigin, computedPosition] = core::ui::anchors::computeAnchorData(
                        textureSize, cfg.sprite.scale, cfg.anchor.anchorType, defaultView);

                    origin = computedOrigin;
                    anchoredPos = computedPosition;
                }

                LOG_TRACE(core::log::cat::Gameplay,
                          "Player sprite: origin=({}, {}), position=({}, {})", origin.x, origin.y,
                          anchoredPos.x, anchoredPos.y);

                // Создаём ID-based sprite component
                core::ecs::SpriteComponent spriteComp{};
                spriteComp.textureId = cfg.sprite.textureId;
                spriteComp.textureRect = {};
                spriteComp.baseScale = cfg.sprite.scale; // IMMUTABLE базовый масштаб
                spriteComp.scale = cfg.sprite.scale;     // текущий (изначально = base)
                spriteComp.origin = origin;
                spriteComp.zOrder = 0.f;

                // ============================================================================
                // КРИТИЧЕСКИЙ ФИКС: правильная инициализация lastUniform
                // ============================================================================

                core::ecs::ScalingBehaviorComponent scalingComp{};
                scalingComp.kind = cfg.anchor.scalingBehavior;
                scalingComp.baseViewSize = mBaseViewSize;

                // ПРАВИЛЬНО: вычисляем ТЕКУЩИЙ uniform factor для начального окна
                // Если окно стартовало с размером != baseViewSize, это учтётся!
                scalingComp.lastUniform =
                    core::ui::computeUniformFactor(defaultView, mBaseViewSize);

                LOG_TRACE(core::log::cat::Gameplay,
                          "ScalingBehavior: baseViewSize=({}, {}), lastUniform={}", mBaseViewSize.x,
                          mBaseViewSize.y, scalingComp.lastUniform);

                // Компонент фиксации
                core::ecs::LockBehaviorComponent lockComp{};
                lockComp.kind = cfg.anchor.lockBehavior;
                lockComp.previousViewSize = mBaseViewSize;
                lockComp.initialized = false;

                // Добавляем все компоненты
                LOG_TRACE(core::log::cat::Gameplay, "Adding components to entity {}",
                          core::ecs::toUint(entity));

                world.addComponent(entity, std::move(spriteComp));
                world.addComponent(entity, core::ecs::TransformComponent{anchoredPos});
                world.addComponent(entity, core::ecs::VelocityComponent{{0.f, 0.f}});
                world.addComponent(entity, std::move(scalingComp));
                world.addComponent(entity, std::move(lockComp));

                world.addComponent(entity, core::ecs::MovementStatsComponent{
                                               cfg.movement.maxSpeed, cfg.movement.acceleration,
                                               cfg.movement.friction});

                world.addComponent(entity, core::ecs::KeyboardControlComponent{
                                               cfg.controls.up, cfg.controls.down,
                                               cfg.controls.left, cfg.controls.right});

                LOG_DEBUG(core::log::cat::Gameplay,
                         "Player entity {} fully initialized (ID-based sprite)",
                         core::ecs::toUint(entity));

                toRemove.push_back(entity);
            }

            for (auto e : toRemove) {
                world.removeComponent<PlayerConfigComponent>(e);
            }

            if (processedCount > 0) {
                LOG_INFO(core::log::cat::Gameplay,
                         "PlayerInitSystem: initialized {} player entities", processedCount);
            } else {
                LOG_DEBUG(core::log::cat::Gameplay,
                          "PlayerInitSystem: no PlayerConfigComponent found (nothing to initialize).");
            }

            mHasRun = true;
        }

        void render(core::ecs::World&, sf::RenderWindow&) override {
        }

      private:
        core::resources::ResourceManager& mResources;
        sf::Vector2f mBaseViewSize;
        bool mHasRun;
    };

} // namespace game::skyguard::ecs