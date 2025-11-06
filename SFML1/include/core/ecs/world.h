#pragma once

#include <memory>
#include <typeindex>
#include <type_traits>
#include <unordered_map>

#include <SFML/Graphics.hpp>

#include "core/ecs/component_storage.h"
#include "core/ecs/registry.h"
#include "core/ecs/system_manager.h"

namespace core::ecs {

    /**
     * @brief Главный координирующий класс ECS.
     *
     * Он:
     *  - создаёт/удаляет сущности (через Registry)
     *  - хранит ВСЕ компоненты (через type-erased map)
     *  - обновляет ВСЕ системы
     */
    class World {
    public:
        World() = default;
        ~World() = default;

        // Запрещаем копирование (из-за unique_ptr внутри)
        World(const World&) = delete;
        World& operator=(const World&) = delete;
        // Разрешаем перемещение
        World(World&&) noexcept = default;
        World& operator=(World&&) noexcept = default;

        // --------------------------- Сущности -----------------------------
        Entity createEntity() {
            return mRegistry.createEntity();
        }

        void destroyEntity(Entity e) {
            // Сначала можно будет удалять компоненты у e
            mRegistry.destroyEntity(e);
            // Позже: пройтись по всем storages и сделать remove(e)
        }

        bool isAlive(Entity e) const noexcept {
            return mRegistry.isAlive(e);
        }

        // --------------------------- Компоненты ---------------------------

        /**
         * @brief Получить (создать при необходимости) хранилище компонента T.
         */
        template <typename T>
        ComponentStorage<T>& storage() {
            // Получение хранилища для компонента T
            const std::type_index key{ typeid(T) }; // typeid(T) даёт тип во время выполнения
                                                    // (TransformComponent, HealthComponent, и т.д.)
                                                    // std::type_index - обёртка, вокруг typeid(T),
                                                    // которая позволяет использовать тип typeid(T)
                                                    // в unordered_map как ключ.
            auto it = mStorages.find(key);
            if (it != mStorages.end()) {
                /**
                 * static_cast преобразует IComponentStorage*, который возвращает it->second.get()
                 * в ComponentStorage<T>*, чтобы компилятор разрешил вызывать методы set, get, remove и т.д.,
                 * (которые определены только в ComponentStorage<T>, поэтому нам нужно преобразовать тип обратно к нему.
                 * (*) звездочка здесь - разыменование указателя, который возвращает static_cast, поскольку нам нужно
                 * вернуть ComponentStorage<T>& storage(); не указатель, а сам объект
                 */
                return *static_cast<ComponentStorage<T>*>(it->second.get());
            }

            // Создание при отсутствии хранилища для компонента T
            auto storage = std::make_unique<ComponentStorage<T>>();
            ComponentStorage<T>* ptr = storage.get();
            /**
             * Добавляет в mStorages новый элемент, где ключ — это тип компонента,
             * представленный через std::type_index (обёртку вокруг typeid(T)),
             * чтобы его можно было безопасно и корректно использовать в unordered_map в качестве ключа,
             * а значение — передача владения только что созданным хранилищем storage.
             */
            mStorages.emplace(key, std::move(storage));
            return *ptr;
        }

        // Добавить компонент сущности в хранилище по rvalue через std::move
        template <typename T>
        T& addComponent(Entity e, T&& value) {
            using PureT = std::remove_reference_t<T>;   // удаляем ссылки
            auto& s = storage<PureT>();
            s.set(e, std::forward<T>(value));
            return *s.get(e);
        }

        // Добавить компонент сущности в хранилище по const ref
        template <typename T>
        T& addComponent(Entity e, const T& value) {
            using PureT = std::remove_reference_t<T>;   // удаляем ссылки
            auto& s = storage<PureT>(); // s - переменная типа ComponentStorage<T>,
                                        // здесь мы вызываем у неё метод storage(),описанный выше
            s.set(e, value);
            return *s.get(e);
        }

        // Получить компонент, если он есть
        template <typename T>
        T* getComponent(Entity e) {
            auto& s = storage<T>();
            return s.get(e);
        }

        // Возвращает указатель на компонент (или nullptr, если компонента нет)
        template <typename T>
        const T* getComponent(Entity e) const {
            auto& s = const_cast<World*>(this)->storage<T>();
            return s.get(e);
        }

        // Вызывает erase в хранилище для этой сущности
        template <typename T>
        void removeComponent(Entity e) {
            auto& s = storage<T>();
            s.remove(e);
        }

        // --------------------------- Системы ------------------------------

        template <typename T, typename... Args>
        T& addSystem(Args&&... args) {
            return mSystems.addSystem<T>(std::forward<Args>(args)...);
        }

        void update(float dt) {
            mSystems.updateAll(*this, dt);
        }

        void render(sf::RenderWindow& window) {
            mSystems.renderAll(*this, window);
        }

        // Доступ к registry — если нужно низкоуровневое
        Registry& registry() noexcept { return mRegistry; }
        const Registry& registry() const noexcept { return mRegistry; }

    private:
        Registry mRegistry;
        SystemManager mSystems;

        /**
         * @brief Словарь всех компонентов
         * - type_index позволяет искать по типу во время выполнения;
         * - unique_ptr хранит указатель на базовый класс (IComponentStorage);
         * - при этом каждый компонентный storage — уникален и типизирован отдельно.
         * Благодаря этому World может хранить компоненты любого типа в одном контейнере
         */
        std::unordered_map<std::type_index, std::unique_ptr<IComponentStorage>> mStorages;
    };

} // namespace core::ecs