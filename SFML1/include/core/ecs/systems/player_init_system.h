// ================================================================================================
// File: core/ecs/systems/player_init_system.h
// Purpose: One-shot system that turns PlayerBlueprint into runtime ECS components
// Used by: World (systems wiring in Game)
// Related headers: core/ecs/components/player_config_component.h,
//                  core/resources/resource_manager.h,
//                  core/ui/anchor_policy.h, core/ui/lock_behavior_factory.h
// ================================================================================================
#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "core/config.h"
#include "core/ecs/components/keyboard_control_component.h"
#include "core/ecs/components/lock_behavior_component.h"
#include "core/ecs/components/movement_stats_component.h"
#include "core/ecs/components/player_config_component.h"
#include "core/ecs/components/scaling_behavior_component.h"
#include "core/ecs/components/sprite_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/components/velocity_component.h"
#include "core/ecs/system.h"
#include "core/resources/resource_manager.h"
#include "core/ui/anchor_policy.h"
#include "core/ui/anchor_utils.h"
#include "core/ui/lock_behavior_factory.h"

namespace core::ecs {

    /**
     * @brief Система инициализации игрока из PlayerConfigComponent.
     *
     * Назначение:
     *  - взять PlayerBlueprint (данные из JSON),
     *  - создать на его основе runtime-компоненты:
     *      * SpriteComponent
     *      * TransformComponent
     *      * VelocityComponent
     *      * MovementStatsComponent
     *      * ScalingBehaviorComponent
     *      * LockBehaviorComponent (по необходимости)
     *      * KeyboardControlComponent
     *  - убрать PlayerConfigComponent после успешной инициализации.
     *
     * Система ожидается как "one-shot": после первого апдейта, когда все сущности
     * созданы, её можно отключить или оставить работать вхолостую (она просто не
     * найдёт больше PlayerConfigComponent в мире).
     */
    class PlayerInitSystem final : public ISystem {
      public:
        explicit PlayerInitSystem(core::resources::ResourceManager& res) : mResources(res) {
        }

        void update(World& world, float) override {
            auto& configs = world.storage<PlayerConfigComponent>();

            std::vector<Entity> toRemove; // копим сущности для безопасного удаления после цикла

            for (const auto& [entity, cfgComp] : configs) {
                const auto& cfg = cfgComp.config;

                // Текстура игрока по TextureID из PlayerBlueprint.
                // Путь для неё берётся из ResourcePaths::get(TextureID) внутри ResourceManager.
                const sf::Texture& texture =
                    mResources.getTexture(cfg.sprite.textureId, true).get();

                // Спрайт
                sf::Sprite tempSprite(texture);
                tempSprite.setScale(cfg.sprite.scale);
                SpriteComponent spriteComp(std::move(tempSprite));

                // Позиция и якорь
                sf::View defaultView(
                    sf::FloatRect({0.f, 0.f}, {static_cast<float>(::config::WINDOW_WIDTH),
                                               static_cast<float>(::config::WINDOW_HEIGHT)}));

                core::ui::AnchorType anchor = core::ui::anchors::fromString(cfg.anchor.anchorName);
                if (anchor != core::ui::AnchorType::None) {
                    core::ui::AnchorPolicy(anchor).apply(spriteComp.sprite, defaultView);
                } else {
                    spriteComp.sprite.setPosition(cfg.anchor.startPosition);
                }

                const sf::Vector2f anchoredPos = spriteComp.sprite.getPosition();

                // Компонент масштабирования
                ScalingBehaviorComponent scalingComp{};
                if (cfg.anchor.scaling == "Uniform") {
                    scalingComp.mode = ScalingBehaviorComponent::Mode::Uniform;
                } else {
                    scalingComp.mode = ScalingBehaviorComponent::Mode::None;
                }

                // Компонент фиксации
                std::unique_ptr<core::ui::ILockPolicy> lock_behavior;
                if (!cfg.anchor.lockBehavior.empty() && cfg.anchor.lockBehavior != "None") {
                    lock_behavior = core::ui::LockBehaviorFactory::create(cfg.anchor.lockBehavior);
                }

                // Добавляем компоненты
                world.addComponent(entity, spriteComp);
                world.addComponent(entity, TransformComponent{anchoredPos});
                world.addComponent(entity, VelocityComponent{{0.f, 0.f}});
                world.addComponent(entity, std::move(scalingComp));
                if (lock_behavior) {
                    world.addComponent(entity, LockBehaviorComponent(std::move(lock_behavior)));
                }

                // Параметры движения из cfg.movement
                world.addComponent(
                    entity, MovementStatsComponent{
                        cfg.movement.speed,        // maxSpeed
                        cfg.movement.acceleration, // acceleration
                        cfg.movement.friction      // friction
                    });

                // Управление с клавиатуры — берём из JSON (или из дефолтов config.h)
                world.addComponent(entity,
                                   KeyboardControlComponent{cfg.controls.up,
                                                            cfg.controls.down,
                                                            cfg.controls.left,
                                                            cfg.controls.right});

                // Отложенное удаление PlayerConfigComponent (чтобы не ломать итератор)
                toRemove.push_back(entity);
            }

            for (auto e : toRemove) {
                world.removeComponent<PlayerConfigComponent>(e);
            }
        }

        void render(World&, sf::RenderWindow&) override {
        }

      private:
        core::resources::ResourceManager& mResources;
    };

} // namespace core::ecs