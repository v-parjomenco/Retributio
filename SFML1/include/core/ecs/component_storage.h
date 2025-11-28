// ================================================================================================
// File: core/ecs/component_storage.h
// Purpose: Per-type component storage (unordered_map<Entity,T> placeholder)
// Used by: World
// Related headers: entity.h, world.h
// Notes: Will be swapped to SparseSet later without API changes.
// ================================================================================================
#pragma once

#include <type_traits>
#include <unordered_map>
#include <utility>

#include "core/ecs/entity.h"

namespace core::ecs {

    // Базовый интерфейс для хранилища любого типа компонентов (type-erasure)
    // Нужен, чтобы World мог хранить разные ComponentStorage<T> в одном unordered_map
    // и объединять в одну структуру
    class IComponentStorage {
      public:
        virtual ~IComponentStorage() = default;

        /**
         * @brief Удалить компонент сущности по её идентификатору.
         *
         * World использует этот интерфейс, чтобы при уничтожении сущности
         * пройтись по всем хранилищам и удалить соответствующие компоненты.
         * Конкретная реализация в ComponentStorage<T> должна быть максимально дешёвой
         * (обычно это просто erase по ключу).
         */
        virtual void remove(Entity e) noexcept = 0;
    };

    // Шаблонное хранилище компонентов ОДНОГО типа (н-р: ComponentStorage<TransformComponent>)
    // Не требует конструктора по умолчанию у T (важно для sf::Sprite / SpriteComponent)
    template <typename T> class ComponentStorage final : public IComponentStorage {
      public:
        // Добавить или заменить компонент по константной ссылке
        void set(Entity e, const T& value) {
            mData.insert_or_assign(e, value);
        }

        // Добавить или заменить компонент по rvalue
        void set(Entity e, T&& value) {
            mData.insert_or_assign(e, std::move(value));
        }

          // Вернуть указатель на компонент, если он есть, иначе nullptr
        [[nodiscard]] T* get(Entity e) noexcept {
            auto it = mData.find(e);
            return (it == mData.end()) ? nullptr : std::addressof(it->second);
        }

        // Вернуть константный указатель на компонент, если он есть, иначе nullptr
        [[nodiscard]] const T* get(Entity e) const noexcept {
            auto it = mData.find(e);
            return (it == mData.end()) ? nullptr : std::addressof(it->second);
        }

        // Удалить компонент у сущности (реализация интерфейса IComponentStorage)
        void remove(Entity e) noexcept override {
            mData.erase(e);
        }

        // Можно итерироваться по всем компонентам этого типа
        auto begin() noexcept {
            return mData.begin();
        }
        auto end() noexcept {
            return mData.end();
        }
        auto begin() const noexcept {
            return mData.begin();
        }
        auto end() const noexcept {
            return mData.end();
        }

        // Даёт возможность системам выбирать меньший из сториджей для итерации (см. MovementSystem)
        [[nodiscard]] std::size_t size() const noexcept {
            return mData.size();
        }

      private:
        std::unordered_map<Entity, T> mData;
    };

} // namespace core::ecs