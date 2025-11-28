// ================================================================================================
// File: core/ecs/registry.h
// Purpose: Thin wrapper over EntityManager (will host storages later)
// Used by: World
// Related headers: entity_manager.h, world.h
// ================================================================================================
#pragma once

#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/ecs/entity.h"
#include "core/ecs/entity_manager.h"

namespace core::ecs {

    /**
     * @brief Простейший Registry — обёртка вокруг EntityManager.
     *
     * Сейчас он почти ничего не делает, кроме как создаёт/удаляет сущности
     * и даёт к ним доступ. Позже сюда добавим:
     *  - хранилища компонентов по типу
     *  - регистрацию систем
     *  - обновление по тегам/маскам
     */
    class Registry {
      public:
        Registry() = default;
        ~Registry() = default;

        // Создаём сущность — просто делегируем менеджеру
        [[nodiscard]] Entity createEntity() {
            return mEntities.create();
        }

        // Удаляем сущность
        void destroyEntity(Entity e) {
            mEntities.destroy(e);
            // Позже здесь же будем удалять компоненты из storage'ей
        }

        // Проверяем, жива ли сущность
        [[nodiscard]] bool isAlive(Entity e) const noexcept {
            return mEntities.isAlive(e);
        }

        // Доступ к живым сущностям — для систем
        [[nodiscard]] const std::vector<Entity>& viewAll() const noexcept {
            return mEntities.dense();
        }

        /**
         * @brief Полная очистка всех сущностей, т.е. "жёсткий сброс мира".
         * Использовать только в двух случаях:
         * - при загрузке нового уровня
         * - при полном рестарте игры
         */
        void clear() {
            mEntities.clear();
            // Позже: чистить также component storages
        }

        // Доступ к менеджеру (если вдруг нужен более низкий уровень)
        [[nodiscard]] EntityManager& entities() noexcept {
            return mEntities;
        }
        [[nodiscard]] const EntityManager& entities() const noexcept {
            return mEntities;
        }

      private:
        EntityManager mEntities;
    };

} // namespace core::ecs