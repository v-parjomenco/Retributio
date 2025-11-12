#pragma once

#include "core/ui/anchor_policy.h"
#include "core/ui/anchor_utils.h"
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
#include "core/ui/lock_behavior_factory.h"
#include "core/resources/resource_manager.h"

namespace core::ecs {

    /**
     * @brief Система инициализации игрока из PlayerConfigComponent.
     *
     * Она создаёт визуальные и логические компоненты (Sprite, Transform, Velocity, ResizeBehavior)
     * на основе данных из JSON (player.json).
     * Работает один раз — при первом обновлении, затем может быть отключена.
     */
    class PlayerInitSystem final : public ISystem {
    public:
        explicit PlayerInitSystem(core::resources::ResourceManager& res) 
            : mResources(res) {}

        void update(World& world, float) override {
            auto& configs = world.storage<PlayerConfigComponent>();

            std::vector<Entity> toRemove; // копим сущности для безопасного удаления после цикла

            for (const auto& [entity, cfgComp] : configs) {
                const auto& cfg = cfgComp.config;

                // Текстура
                const sf::Texture& texture =
                    mResources.getTextureByPath(cfg.texturePath, true).get();

                // Спрайт
                sf::Sprite tempSprite(texture);
                tempSprite.setScale(cfg.scale);
                SpriteComponent spriteComp(std::move(tempSprite));

                // Позиция и якорь
                sf::View defaultView(sf::FloatRect({ 0.f, 0.f },
                    { static_cast<float>(::config::WINDOW_WIDTH),
                      static_cast<float>(::config::WINDOW_HEIGHT) }));

                core::ui::AnchorType anchor = core::ui::anchors::fromString(cfg.anchor);
                if (anchor != core::ui::AnchorType::None)
                    core::ui::AnchorPolicy(anchor).apply(spriteComp.sprite, defaultView);
                else
                    spriteComp.sprite.setPosition(cfg.startPosition);

                const sf::Vector2f anchoredPos = spriteComp.sprite.getPosition();

                // Компонент масштабирования
                ScalingBehaviorComponent scalingComp{};
                if (cfg.scaling == "Uniform")
                    scalingComp.mode = ScalingBehaviorComponent::Mode::Uniform;
                else
                    scalingComp.mode = ScalingBehaviorComponent::Mode::None;

                // Компонент фиксации
                std::unique_ptr<ui::ILockPolicy> lock_behavior;
                if (!cfg.lockBehavior.empty() && cfg.lockBehavior != "None") {
                    lock_behavior = core::ui::LockBehaviorFactory::create(cfg.lockBehavior);
                }

                // Добавляем компоненты
                world.addComponent(entity, spriteComp);
                world.addComponent(entity, TransformComponent{ anchoredPos });
                world.addComponent(entity, VelocityComponent{ {0.f, 0.f} });
                world.addComponent(entity, std::move(scalingComp));
                if (lock_behavior) {
                    world.addComponent(entity, LockBehaviorComponent(std::move(lock_behavior)));
                }

                // Параметры движения из JSON (скорость и др.)
                world.addComponent(entity, MovementStatsComponent{
                    cfg.speed,           // maxSpeed
                    800.f,               // acceleration (пока фикс — можно вынести в JSON позднее)
                    6.f                  // friction    (пока фикс — можно вынести в JSON позднее)
                });

                // Управление с клавиатуры — берём из JSON (или из дефолтов config.h)
                world.addComponent(entity, KeyboardControlComponent{
                    cfg.controls.up,
                    cfg.controls.down,
                    cfg.controls.left,
                    cfg.controls.right
                });

                // Отложенное удаление PlayerConfigComponent (чтобы не ломать итератор)
                toRemove.push_back(entity);
            }

            for (auto e : toRemove) {
                world.removeComponent<PlayerConfigComponent>(e);
            }
        }

        void render(World&, sf::RenderWindow&) override {}

    private:
        core::resources::ResourceManager& mResources;
    };

} // namespace core::ecs