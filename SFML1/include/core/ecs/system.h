// ================================================================================================
// File: core/ecs/system.h
// Purpose: Base interface for ECS systems.
// Used by: core/ecs/system_manager.h, core/ecs/world.h, all concrete systems.
// ================================================================================================
#pragma once

namespace sf {
    class RenderWindow;
} // namespace sf

namespace core::ecs {

    class World; // предварительное объявление класса (forward declaration),
                 // чтобы не тянуть весь world.h сюда.

    /**
     * @brief Базовый интерфейс ECS-системы.
     *
     * Cистемы:
     *  - не хранят данные
     *  - ничего не знают про конкретные сущности, только про их компоненты
     *  - не владеют ресурсами: они должны быть переданы в конструктор извне (DI).
     *
     * Комментарии по контракту:
     *  - update() вызывается каждый тик (или каждый кадр, зависит от Game).
     *  - render() вызывается в рендер-фазе, после update().
     */
    class ISystem {
      public:
        virtual ~ISystem() = default;

        virtual void update(World& world, float dt) {
            (void)world;
            (void)dt;
        }

        virtual void render(World& world, sf::RenderWindow& window) {
            (void)world;
            (void)window;
        }
    };

} // namespace core::ecs
