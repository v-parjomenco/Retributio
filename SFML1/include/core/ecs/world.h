// ================================================================================================
// File: core/ecs/world.h
// Purpose: Zero-overhead ECS façade over EnTT (entt::registry) + SystemManager.
// Used by: Game layer, all ECS systems.
// Related headers: core/ecs/entity.h, core/ecs/system_manager.h
//
// Notes:
//  - Single-threaded for now. No method here is thread-safe by itself.
//  - group() is intentionally NON-const: EnTT may need to mutate internal state to form a group.
//    Read-only code should use view() const.
// ================================================================================================
#pragma once

#include <cassert>
#include <concepts>
#include <type_traits>
#include <utility>

#include "third_party/entt/entt_registry.hpp"

#include "core/ecs/entity.h"
#include "core/ecs/system_manager.h"

namespace sf {
    class RenderWindow;
} // namespace sf

namespace core::ecs {

    /**
     * @brief Политика допустимого ECS-компонента (compile-time фильтр).
     *
     * Требования проекта:
     *  - POD-like: trivially copyable (SoA-friendly, быстрая сериализация);
     *  - "Голый" тип: без const/volatile и без ссылок (EnTT хранит не-cvref типы);
     *  - Без владения ресурсами: trivially destructible (никаких нетривиальных деструкторов).
     */
    template <typename T>
    concept Component =
        std::is_trivially_copyable_v<T> &&
        std::is_trivially_destructible_v<T> &&
        !std::is_const_v<T> &&
        !std::is_volatile_v<T> &&
        !std::is_reference_v<T>;

    /**
     * @brief Главный координатор ECS (тонкий фасад над entt::registry).
     *
     * Цели дизайна:
     *  - Zero overhead: всё инлайнится и сводится к прямым вызовам EnTT;
     *  - Fail-fast в Debug: assert на инварианты, нулевой cost в Release;
     *  - Жёсткая политика компонентов: концепт Component режет неподходящие типы на компиляции.
     */
    class World {
      public:
        World() = default;
        ~World() = default;

        // World — единственный владелец реестра, копирование запрещаем.
        World(const World&) = delete;
        World& operator=(const World&) = delete;

        // Перемещение допустимо (например, Game хранит World полем).
        World(World&&) = default;
        World& operator=(World&&) = default;

        // ----------------------------------------------------------------------------------------
        // Жизненный цикл сущностей
        // ----------------------------------------------------------------------------------------

        /**
         * @brief Создать новую сущность.
         *
         * Сложность: O(1)
         * Thread-safety: Небезопасно.
         */
        [[nodiscard]] Entity createEntity() {
            return mRegistry.create();
        }

        /**
         * @brief Уничтожить сущность и все её компоненты.
         *
         * Сложность: O(N), где N — количество типов компонентов у сущности.
         * Thread-safety: Небезопасно.
         */
        void destroyEntity(Entity e) {
            assert(isAlive(e) && "destroyEntity: entity must be alive");
            mRegistry.destroy(e);
        }

        /**
         * @brief Проверить, валидна ли сущность.
         *
         * Сложность: O(1)
         * Thread-safety: Безопасно для чтения (если нет параллельных create/destroy).
         */
        [[nodiscard]] bool isAlive(Entity e) const
            noexcept(noexcept(mRegistry.valid(e))) {
            return mRegistry.valid(e);
        }

        // ----------------------------------------------------------------------------------------
        // Компоненты
        // ----------------------------------------------------------------------------------------

        /**
         * @brief Добавить или заменить компонент (in-place construction).
         *
         * Политика детерминированности:
         *  - Для НЕ-пустых компонентов запрещён вызов addComponent<T>(e) без аргументов.
         *    Это блокирует случайные "пустые вставки" и скрытые неинициализации.
         *  - Для tag-компонентов (empty type) addComponent<Tag>(e) разрешён.
         *  - Если нужна дефолтная инициализация data-компонента — делай это ЯВНО:
         *      world.addComponent<Health>(e, Health{});
         *
         * Сложность: O(1) amortized.
         * Thread-safety: Небезопасно.
         */
        template <Component T, typename... Args>
            requires std::constructible_from<T, Args...>
        T& addComponent(Entity e, Args&&... args) {
            static_assert(sizeof...(Args) > 0 || std::is_empty_v<T>,
                          "World::addComponent<T>: non-empty components must be explicitly initialized. "
                          "Use addComponent<T>(e, T{}) for default initialization.");

            assert(isAlive(e) && "addComponent: entity must be alive");
            return mRegistry.emplace_or_replace<T>(e, std::forward<Args>(args)...);
        }

        /**
         * @brief Проверить наличие компонента у сущности.
         *
         * ВАЖНО: Если сразу после проверки нужен доступ к компоненту,
         *        используй tryGetComponent() — это быстрее (один lookup вместо двух).
         *
         * Сложность: O(1)
         * Thread-safety: Безопасно для чтения.
         */
        template <Component T>
        [[nodiscard]] bool hasComponent(Entity e) const
            noexcept(noexcept(mRegistry.any_of<T>(e))) {
            assert(isAlive(e) && "hasComponent: entity must be alive");
            return mRegistry.any_of<T>(e);
        }

        /**
         * @brief Безопасное получение: nullptr если сущность невалидна или компонента нет.
         *
         * КРИТИЧНО ДЛЯ ПРОИЗВОДИТЕЛЬНОСТИ:
         *  Делает ОДИН lookup вместо двух при паттерне "проверить и получить":
         *
         *  ❌ Медленно (2 lookup):
         *      if (world.hasComponent<Health>(e)) {
         *          auto& hp = world.getComponent<Health>(e);
         *      }
         *
         *  ✅ Быстро (1 lookup):
         *      if (auto* hp = world.tryGetComponent<Health>(e)) {
         *          hp->value -= damage;
         *      }
         *
         * Сложность: O(1)
         * Thread-safety: Небезопасно (эксклюзивный доступ на запись).
         */
        template <Component T>
        [[nodiscard]] T* tryGetComponent(Entity e)
            noexcept(noexcept(mRegistry.try_get<T>(e))) {
            return mRegistry.try_get<T>(e);
        }

        /**
         * @brief Константная версия tryGetComponent.
         *
         * Сложность: O(1)
         * Thread-safety: Безопасно для параллельного чтения.
         */
        template <Component T>
        [[nodiscard]] const T* tryGetComponent(Entity e) const
            noexcept(noexcept(mRegistry.try_get<T>(e))) {
             return mRegistry.try_get<T>(e);
        }

        /**
         * @brief Жёсткое получение: assert если сущность невалидна или компонента нет.
         *
         * Реализация через try_get + assert:
         *  - Debug: один lookup и понятное сообщение;
         *  - Release: тот же контракт (не используешь — получишь UB), без лишнего оверхеда.
         *
         * Сложность: O(1)
         * Thread-safety: Небезопасно (эксклюзивный доступ на запись).
         */
        template <Component T>
        [[nodiscard]] T& getComponent(Entity e) {
            assert(isAlive(e) && "getComponent: entity must be alive");
            auto* ptr = mRegistry.try_get<T>(e);
            assert(ptr && "getComponent: component must exist");
            return *ptr;
        }

        /**
         * @brief Константная версия getComponent.
         *
         * Сложность: O(1)
         * Thread-safety: Безопасно для параллельного чтения.
         */
        template <Component T>
        [[nodiscard]] const T& getComponent(Entity e) const {
            assert(isAlive(e) && "getComponent: entity must be alive");
            const auto* ptr = mRegistry.try_get<T>(e);
            assert(ptr && "getComponent: component must exist");
            return *ptr;
        }

        /**
         * @brief Удалить компонент у сущности (если он присутствует).
         *
         * Сложность: O(1)
         * Thread-safety: Небезопасно.
         */
        template <Component T>
        void removeComponent(Entity e)
            noexcept(noexcept(mRegistry.remove<T>(e))) {
            assert(isAlive(e) && "removeComponent: entity must be alive");
            mRegistry.remove<T>(e);
        }

        // ----------------------------------------------------------------------------------------
        // Запросы (view / group)
        // ----------------------------------------------------------------------------------------

        /**
         * @brief EnTT view для итерации по сущностям с заданным набором компонентов.
         *
         * Пример использования:
         *  for (auto [entity, pos, vel] : world.view<Position, Velocity>().each()) {
         *      pos.x += vel.dx * dt;
         *  }
         *
         * Сложность итерации: O(N), где N — количество сущностей с наименьшим компонентом.
         * Thread-safety: Небезопасно (если есть параллельные модификации).
         */
        template <Component... Components>
        [[nodiscard]] auto view() {
            return mRegistry.view<Components...>();
        }

        /**
         * @brief Константный view для read-only систем.
         *
         * EnTT автоматически превращает view<T> в view<const T> для const registry.
         *
         * Thread-safety: Безопасно для параллельного чтения (если нет write-операций).
         */
        template <Component... Components>
        [[nodiscard]] auto view() const {
            return mRegistry.view<Components...>();
        }

        /**
         * @brief EnTT group для горячих путей (рендер, физика).
         *
         * ВАЖНО:
         *  - Метод намеренно НЕ const.
         *    В EnTT формирование/поддержка group может требовать внутренних мутаций реестра.
         *  - Для read-only кода на const World используй view() const.
         *
         * Пример:
         *  auto group = world.group<Position, Velocity>(entt::get<Sprite>);
         *  // Position и Velocity хранятся плотно, Sprite — отдельно.
         *
         * Сложность: O(1) для получения, O(N) для первой итерации (сортировка).
         * Thread-safety: Небезопасно.
         */
        template <Component... Owned, Component... Get>
        [[nodiscard]] auto group(entt::get_t<Get...> get = entt::get_t<Get...>{}) {
            return mRegistry.group<Owned...>(get);
        }

        // ----------------------------------------------------------------------------------------
        // Системы
        // ----------------------------------------------------------------------------------------

        /**
         * @brief Добавить систему в мир.
         *
         * Системы вызываются в порядке добавления.
         */
        template <typename T, typename... Args>
        T& addSystem(Args&&... args) {
            return mSystems.addSystem<T>(std::forward<Args>(args)...);
        }

        /**
         * @brief Обновить все системы (логическая часть игры).
         */
        void update(float dt) {
            mSystems.updateAll(*this, dt);
        }

        /**
         * @brief Отрисовать все системы (рендеринг).
         */
        void render(sf::RenderWindow& window) {
            mSystems.renderAll(*this, window);
        }

        // ----------------------------------------------------------------------------------------
        // Escape hatch
        // ----------------------------------------------------------------------------------------

        /**
         * @brief Прямой доступ к entt::registry для продвинутых случаев.
         *
         * ИСПОЛЬЗУЙ С ОСТОРОЖНОСТЬЮ:
         *  - Обходит все проверки World;
         *  - Нужен только для сложных EnTT-фич (observers, signals и т.п.);
         *  - В обычном коде используй методы World.
         */
        [[nodiscard]] entt::registry& registry() noexcept {
            return mRegistry;
        }

        [[nodiscard]] const entt::registry& registry() const noexcept {
            return mRegistry;
        }

      private:
        entt::registry mRegistry{};
        SystemManager mSystems{};
    };

} // namespace core::ecs