// TODO: позже заменить на SparseSet для оптимизации под массовые сущности
/*
Прежде чем прыгать в SparseSet-хранилище, нужно:

Подключить систему (SystemManager)
— чтобы у нас появились "логические процессы", которые работают с компонентами (например, MovementSystem, RenderSystem).

Создать World (или ECSWorld)
— он будет координировать всё:
entities, components, systems.

Добавить обновление (update loop)
— чтобы каждый кадр ECS мир вызывал все активные системы и они работали с компонентами.

И только после этого, когда всё функционально работает,
мы заменим unordered_map на SparseSet, не меняя интерфейса.
*/

#pragma once

#include <unordered_map>

#include "core/ecs/entity.h"

namespace core::ecs {

    // Базовый интерфейс для хранилища любого типа компонентов (type-erasure)
    // Нужен, чтобы World мог хранить разные ComponentStorage<T> в одном unordered_map и объединять в одну структуру
    class IComponentStorage {
    public:
        virtual ~IComponentStorage() = default;
        // Позже можно добавить сюда:
        // virtual void onEntityDestroyed(Entity e) = 0; // удаляем все компоненты у сущности
    };

    // Шаблонное хранилище компонентов ОДНОГО типа (н-р: ComponentStorage<TransformComponent>)
    // Не требует конструктора по умолчанию у T(важно для sf::Sprite / SpriteComponent)

    template <typename T>
    class ComponentStorage final : public IComponentStorage {
    public:
        // Добавить или заменить компонент
        void set(Entity e, const T& value) {
            mData.insert_or_assign(e, value);
        }

        void set(Entity e, T&& value) {
            mData.insert_or_assign(e, std::move(value));
        }

        // Вернуть указатель на компонент, если он есть, иначе вернуть nullptr
        T* get(Entity e) {
            auto it = mData.find(e);
            return (it == mData.end()) ? nullptr : &it->second;
        }

        // Вернуть константный указатель на компонент, если он есть, иначе вернуть nullptr
        const T* get(Entity e) const {
            auto it = mData.find(e);
            return (it == mData.end()) ? nullptr : &it->second;
        }

        // Удалить компонент у сущности
        void remove(Entity e) {
            mData.erase(e);
        }

        // Можно итерироваться по всем компонентам этого типа
        auto begin() { return mData.begin(); }
        auto end()   { return mData.end(); }
        auto begin() const { return mData.begin(); }
        auto end()   const { return mData.end(); }

    private:
        std::unordered_map<Entity, T> mData;
    };

} // namespace core::ecs