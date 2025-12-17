// ================================================================================================
// File: core/ecs/world.h
// Purpose: ECS coordinator facade over entt::registry
// Used by: Game, all systems
// Related headers: entity.h, system_manager.h, third_party/entt/entt_registry.hpp
// ================================================================================================
#pragma once

#include <type_traits>
#include <utility>

#include <SFML/Graphics.hpp>

#include "third_party/entt/entt_registry.hpp"

#include "core/ecs/entity.h"
#include "core/ecs/system_manager.h"

namespace core::ecs {

    namespace detail {

        /**
         * @brief Вспомогательный трейт для проверки "чистого" типа компонента.
         *
         * В терминах EnTT компонент должен быть:
         *  - без const;
         *  - без volatile;
         *  - без ссылок (&, &&).
         *
         * Мы не навязываем здесь POD/TriviallyCopyable (это отдельный уровень политики),
         * а проверяем только "форму" типа, чтобы ошибка всплывала в нашем коде,
         * а не где-то глубоко в EnTT/ STL (xmemory и т.п.).
         */
        template <typename T>
        struct is_bare_component : std::bool_constant<
                                      !std::is_const_v<T> &&
                                      !std::is_volatile_v<T> &&
                                      !std::is_reference_v<T>> {
        };

        template <typename T>
        inline constexpr bool is_bare_component_v = is_bare_component<T>::value;

    } // namespace detail

    /**
     * @brief Главный координирующий класс ECS (EnTT backend).
     *
     * Принципы дизайна:
     *  - "Нулевая" обёртка над entt::registry (минимальный оверхед);
     *  - все операции с компонентами инлайн и сводятся к прямым вызовам EnTT;
     *  - интерфейс World максимально простой и дружественный к job-системе;
     *  - системы используют view<>/group<> (нативные идиомы EnTT).
     */
    class World {
      public:
        World() = default;
        ~World() = default;

        World(const World&) = delete;
        World& operator=(const World&) = delete;

        // Движок допускает перемещение World (например, Game владеет им как полем).
        World(World&&) = default;
        World& operator=(World&&) = default;

        // ------------------------------- Жизненный цикл сущности --------------------------------

        /**
         * @brief Создать новую сущность.
         *
         * Возвращает валидный дескриптор Entity, который затем можно использовать
         * для добавления компонентов.
         */
        [[nodiscard]] Entity createEntity() {
            return mRegistry.create();
        }

        /**
         * @brief Уничтожить сущность и все её компоненты.
         */
        void destroyEntity(Entity e) {
            mRegistry.destroy(e);
        }

        /**
         * @brief Проверить, жива ли сущность (валиден ли дескриптор).
         */
        [[nodiscard]] bool isAlive(Entity e) const noexcept {
            return mRegistry.valid(e);
        }

        // --------------------------------- Доступ к компонентам ---------------------------------

        /**
         * @brief Добавить или заменить компонент у сущности.
         *
         * Единственный overload addComponent:
         *  - lvalue → копирование;
         *  - rvalue (std::move / временный объект) → перемещение;
         *  - const& / volatile& → копирование в "голый" тип компонента (EnTT хранит T, не cvref).
         *
         * Важно:
         *  - Внутренний тип компонента для EnTT — это всегда
         *    std::remove_cv_t<std::remove_reference_t<T>>.
         *  - Это позволяет писать удобный код на уровне вызова,
         *    не задумываясь о cv/ref в конкретном месте.
         */
        template <typename T>
        decltype(auto) addComponent(Entity e, T&& value) {
            using ComponentType = std::remove_cv_t<std::remove_reference_t<T>>;

            // Эта проверка держит контракт "хранимый тип компонента должен быть bare".
            // (На практике remove_cv/remove_reference уже обеспечивает bare, но assert оставляем
            // чтобы политика была явно зафиксирована в одном месте.)
            static_assert(
                detail::is_bare_component_v<ComponentType>,
                "World::addComponent(): stored ECS component type must be non-const, non-volatile,"
                " and non-reference"
            );

            return mRegistry.emplace_or_replace<ComponentType>(e, std::forward<T>(value));
        }

        /**
         * @brief Получить указатель на изменяемый компонент (или nullptr, если компонента нет).
         */
        template <typename T>
        [[nodiscard]] T* getComponent(Entity e) {
            static_assert(
                detail::is_bare_component_v<T>,
                "World::getComponent<T>(): T must be a bare component type (no const/volatile/&). "
                "Use getComponent<Foo>(), not getComponent<const Foo>() or getComponent<Foo&>()"
            );
            return mRegistry.try_get<T>(e);
        }

        /**
         * @brief Получить указатель на константный компонент (или nullptr, если компонента нет).
         */
        template <typename T>
        [[nodiscard]] const T* getComponent(Entity e) const {
            static_assert(
                detail::is_bare_component_v<T>,
                "World::getComponent<T>() const: T must be a bare component type "
                "(no const/volatile/&). "
                "Use getComponent<Foo>(), not getComponent<const Foo>() or getComponent<Foo&>()"
            );
            return mRegistry.try_get<T>(e);
        }

        /**
         * @brief Удалить компонент у сущности (если он был).
         */
        template <typename T>
        void removeComponent(Entity e) {
            static_assert(
                detail::is_bare_component_v<T>,
                "World::removeComponent<T>(): T must be a bare component type"
                "(no const/volatile/&). Use removeComponent<Foo>(), "
                "not removeComponent<const Foo>() or removeComponent<Foo&>()"
            );
            mRegistry.remove<T>(e);
        }

        // ---------------------------- Запросы систем через EnTT view ----------------------------

        /**
         * @brief EnTT view для итерации по сущностям с заданным набором компонентов.
         *
         * Критично:
         *  - Components... должны быть "голыми" типами (TransformComponent, и т.п.);
         *  - НЕ использовать const T, volatile T, T&, const T& в списке шаблонных параметров.
         */
        template <typename... Components>
        [[nodiscard]] auto view() {
            static_assert(
                (detail::is_bare_component_v<Components> && ...),
                "World::view<Components...>(): all component types must be bare "
                "(no const/volatile/&). "
                "Use view<Foo, Bar>(), not view<const Foo>() or view<Foo&>()"
            );
            return mRegistry.view<Components...>();
        }

        template <typename... Components>
        [[nodiscard]] auto view() const {
            static_assert(
                (detail::is_bare_component_v<Components> && ...),
                "World::view<Components...>() const: all component types must be bare "
                "(no const/volatile/&). "
                "Use view<Foo, Bar>(), not view<const Foo>() or view<Foo&>()"
            );
            return mRegistry.view<Components...>();
        }

        /**
         * @brief EnTT group для "горячих" путей (рендер, физика и т.п.).
         *
         * Owned... — компоненты, которые принадлежат группе
         *            (EnTT может их упорядочивать и хранить плотнее).
         * Get...   — компоненты, которые лишь читаются вместе с Owned.
         */
        template <typename... Owned, typename... Get>
        [[nodiscard]] auto group(entt::get_t<Get...> = {}) {
            static_assert(
                (detail::is_bare_component_v<Owned> && ...),
                "World::group<Owned...>(...): Owned... must be bare component types "
                "(no const/volatile/&)"
            );
            static_assert(
                (detail::is_bare_component_v<Get> && ...),
                "World::group<...>(entt::get<Get...>): Get... must be bare component types "
                "(no const/volatile/&)"
            );

            return mRegistry.group<Owned...>(entt::get<Get...>);
        }

        // --------------------------------- Управление системами ---------------------------------

        /**
         * @brief Добавить систему в мир.
         *
         * Система создаётся внутри SystemManager и будет участвовать
         * в update()/render() в порядке добавления.
         */
        template <typename T, typename... Args>
        T& addSystem(Args&&... args) {
            return mSystems.addSystem<T>(std::forward<Args>(args)...);
        }

        /**
         * @brief Обновить все системы (логическая часть).
         */
        void update(float dt) {
            mSystems.updateAll(*this, dt);
        }

        /**
         * @brief Отрисовать все системы, которые что-либо рисуют.
         */
        void render(sf::RenderWindow& window) {
            mSystems.renderAll(*this, window);
        }

        // ------------------------------- Прямой доступ к registry -------------------------------

        /**
         * @brief Прямой доступ к entt::registry (escape hatch для сложных случаев).
         *
         * В обычном коде по возможности использовать методы World:
         *  - createEntity / destroyEntity;
         *  - addComponent / getComponent / removeComponent;
         *  - view / group.
         */
        [[nodiscard]] entt::registry& registry() noexcept {
            return mRegistry;
        }

        [[nodiscard]] const entt::registry& registry() const noexcept {
            return mRegistry;
        }

      private:
        entt::registry mRegistry;
        SystemManager mSystems;
    };

} // namespace core::ecs