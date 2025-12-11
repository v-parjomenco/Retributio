// ================================================================================================
// File: game/skyguard/ecs/systems/player_init_system.h
// Purpose: One-shot system that turns PlayerBlueprint into runtime ECS components
// Used by: World (systems wiring in Game)
// Related headers: game/skyguard/ecs/components/player_config_component.h,
//                  game/skyguard/config/blueprints/player_blueprint.h,
//                  core/resources/resource_manager.h,
//                  core/ui/anchor_policy.h
// ================================================================================================
#pragma once

#include <utility>
#include <vector>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>

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
#include "core/ui/anchor_policy.h"
#include "core/ui/anchor_utils.h"
#include "core/ui/scaling_behavior.h"

#include "game/skyguard/ecs/components/player_config_component.h"

namespace game::skyguard::ecs {

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
     *      * LockBehaviorComponent
     *      * KeyboardControlComponent
     *  - убрать PlayerConfigComponent после успешной инициализации.
     *
     * Система ожидается как "one-shot": после первого апдейта, когда все сущности
     * созданы, её можно отключить или оставить работать вхолостую (она просто не
     * найдёт больше PlayerConfigComponent в мире).
     */
    class PlayerInitSystem final : public core::ecs::ISystem {
      public:
        explicit PlayerInitSystem(core::resources::ResourceManager& res, sf::Vector2f baseViewSize)
            : mResources(res), mBaseViewSize(baseViewSize) {
        }

        void update(core::ecs::World& world, float) override {
            auto& configs = world.storage<PlayerConfigComponent>();

            // Копим сущности для безопасного удаления после цикла
            std::vector<core::ecs::Entity> toRemove;
            toRemove.reserve(configs.size());

            for (const auto& [entity, cfgComp] : configs) {
                const auto& cfg = cfgComp.config;

                // Текстура игрока по TextureID из PlayerBlueprint.
                // Путь и флаги (smooth/repeated/mipmap) берутся из ResourcePaths::TextureConfig
                // внутри ResourceManager.
                const sf::Texture& texture = mResources.getTexture(cfg.sprite.textureId).get();

                // Спрайт
                sf::Sprite tempSprite(texture);
                tempSprite.setScale(cfg.sprite.scale);
                core::ecs::SpriteComponent spriteComp(std::move(tempSprite));

                // Позиция и якорь
                // Базовая view для якоря и масштабирования — берём размер,
                // переданный из Game (во время первичной инициализации окна).
                sf::View defaultView(sf::FloatRect({0.f, 0.f}, {mBaseViewSize.x, mBaseViewSize.y}));
                const sf::Vector2f baseViewSize = defaultView.getSize();

                // Используем enum AnchorType из AnchorProperties, без строк и JSON.
                const core::ui::AnchorType anchorType = cfg.anchor.anchorType;
                if (anchorType != core::ui::AnchorType::None) {
                    core::ui::AnchorPolicy(anchorType).apply(spriteComp.sprite, defaultView);
                } else {
                    spriteComp.sprite.setPosition(cfg.anchor.startPosition);
                }

                const sf::Vector2f anchoredPos = spriteComp.sprite.getPosition();

                // Компонент масштабирования (enum уже пришёл из loader'а)
                core::ecs::ScalingBehaviorComponent scalingComp{};
                scalingComp.kind = cfg.anchor.scalingBehavior;
                scalingComp.baseViewSize = baseViewSize;
                scalingComp.lastUniform = 1.f; // "без дополнительного масштабирования"

                // Компонент фиксации (enum из loader'а)
                core::ecs::LockBehaviorComponent lockComp{};
                lockComp.kind = cfg.anchor.lockBehavior;
                lockComp.previousViewSize = baseViewSize;
                lockComp.initialized = false; // первый resize ещё не был обработан

                // Добавляем компоненты
                world.addComponent(entity, spriteComp);
                world.addComponent(entity, core::ecs::TransformComponent{anchoredPos});
                world.addComponent(entity, core::ecs::VelocityComponent{{0.f, 0.f}});
                world.addComponent(entity, std::move(scalingComp));
                world.addComponent(entity, lockComp);

                // Параметры движения из cfg.movement
                world.addComponent(entity, core::ecs::MovementStatsComponent{
                                               cfg.movement.maxSpeed,
                                               cfg.movement.acceleration,
                                               cfg.movement.friction
                                           });

                // Управление с клавиатуры — берём из JSON (или из дефолтов properties)
                world.addComponent(entity, core::ecs::KeyboardControlComponent{
                                               cfg.controls.up, cfg.controls.down,
                                               cfg.controls.left, cfg.controls.right});

                // Отложенное удаление PlayerConfigComponent (чтобы не ломать итератор)
                toRemove.push_back(entity);
            }

            for (auto e : toRemove) {
                world.removeComponent<PlayerConfigComponent>(e);
            }
        }

        void render(core::ecs::World&, sf::RenderWindow&) override {
        }

      private:
        core::resources::ResourceManager& mResources;
        sf::Vector2f mBaseViewSize;
    };

} // namespace game::skyguard::ecs
