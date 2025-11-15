// =====================================================
// File: core/ecs/system.h
// Purpose: Base interface for systems (update/render)
// Used by: SystemManager, concrete systems
// Related headers: system_manager.h, world.h
// =====================================================
#pragma once

#include <SFML/Graphics.hpp>

namespace core::ecs {

    class
        World; // предварительное объявление класса (forward declaration), чтобы компилятор знал, что такой класс есть
    // но ему не нужно знать, что у него внутри, поэтому мы не подключаем его, через include,
    // чтобы избежать цепочку рекурсивных зависимостей между заголовками

    /**
     * @brief Базовый интерфейс для всех систем ECS.
     *
     * Система — это "кусок логики", который:
     *  - не хранит данные
     *  - работает поверх компонентов в World
     *  - ничего не знает про конкретные сущности, только про их компоненты
     */
    class ISystem {
      public:
        virtual ~ISystem() = default;

        /**
         * @brief Логическое обновление (каждый кадр / с фиксированным шагом)
         * @param world — ECS-мир, из которого можно забрать компоненты
         * @param dt    — дельта-время в секундах
         */
        virtual void update(World& world, float dt) {
            // по умолчанию ничего не делаем
            (void) world;
            (void) dt;
        }

        /**
         * @brief Отрисовка (не всем системам нужна, но удобно иметь)
         */
        virtual void render(World& world, sf::RenderWindow& window) {
            // по умолчанию ничего не делаем
            (void) world;
            (void) window;
        }
    };

} // namespace core::ecs