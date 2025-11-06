#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

#include "core/ecs/entity.h"

namespace core::ecs {

    // Менеджер сущностей на основе sparse set.
    class EntityManager {
    public:
        using EntityId = std::uint32_t;

        // Неверный id означает, что такой сущности в мире не бывает
        static constexpr EntityId kInvalidId = 0;

        EntityManager() = default;
        ~EntityManager() = default;

        /**
         * @brief Создать новую сущность.
         * @return core::ecs::Entity — всегда валидный (id != 0)
         */
        Entity create() {
            EntityId id;

            // Eсли есть свободные id в списке свободных id mFreeList — берём оттуда
            if (!mFreeList.empty()) {
                id = mFreeList.back();
                mFreeList.pop_back();
            }
            else {
                // Иначе выдаём новый id
                id = static_cast<EntityId>(mDense.size() + 1); // +1 чтобы id = 0 осталось "пустым"
                // Гарантируем, что в разряжённом векторе mSparse достаточно места
                if (id >= mSparse.size()) {
                    mSparse.resize(id + 1, kInvalidIndex); // меняем размер, заполняя пустые элементы kInvalidIndex
                }
            }

            const std::size_t denseIndex = mDense.size(); // denseIndex - индекс сущности в плотном массиве, т.е.
                                                          // место, в конце вектора mDense, куда добавляем новую сущность
            mDense.push_back(Entity{ id });
            mSparse[id] = static_cast<EntityId>(denseIndex); // добавляем id сущности, которую положили mDense в mSparse

            return Entity{ id };
        }

        /**
         * @brief Удалить сущность.
         * Безопасно вызывать даже если уже удалена — метод просто вернёт.
         */
        void destroy(Entity e) {
            const EntityId id = e.id;
            if (!isAlive(e)) {
                return; // уже нет
            }

            // Индекс сущности в плотном векторе = id в разряженном
            const std::size_t denseIndex = mSparse[id];
            const std::size_t lastIndex = mDense.size() - 1;

            // Если удаляем не последний элемент из вектора mDense — меняем его местами с последним
            if (denseIndex != lastIndex) {
                Entity lastEntity = mDense[lastIndex];
                mDense[denseIndex] = lastEntity;
                // Обновляем также id в векторе mSparse, чтобы оно соответствовало перестанавленному элементу
                mSparse[lastEntity.id] = static_cast<EntityId>(denseIndex);
            }

            // Удаляем физически последний элемент вектора mDense, который мы уже переместили
            mDense.pop_back();
            // Помечаем mSparse-ячейку, где он был прежде как пустую
            mSparse[id] = kInvalidIndex;
            // Кладём id в список свободных id
            mFreeList.push_back(id);
        }

        /**
         * @brief Проверка, жива ли сущность.
         * Работает за O(1).
         */
        bool isAlive(Entity e) const noexcept {
            const EntityId id = e.id;
            // Сразу отсекаем неверные или нулевые id
            if (id == kInvalidId || id >= mSparse.size()) {
                return false;
            }
            const EntityId denseIndex = mSparse[id]; // Смотрим, что хранится в mSparse по этому id
            if (denseIndex == kInvalidIndex) {       // Если там kInvalidIndex, значит сейчас нет такого живого элемента
                return false;
            }
            // Доп. защита от рассинхрона между mDense и mSparse,
            // н-р, на случай, если в mDense окажется меньше элементов, чем id в mSparse (denseIndex),
            // чтобы избежать обращения к невалидной памяти
            return denseIndex < mDense.size()
                && mDense[denseIndex].id == id;
        }

        /**
         * @brief Доступ к плотному вектору сущностей — для Registry / Systems.
         * Здесь они идут без дырок.
         */
        const std::vector<Entity>& dense() const noexcept {
            return mDense;
        }

        /**
         * @brief Текущее количество живых сущностей.
         */
        std::size_t aliveCount() const noexcept {
            return mDense.size();
        }

        /**
         * @brief Полная очистка всех сущностей, т.е. "жёсткий сброс мира".
         * Использовать только в двух случаях:
         * - при загрузке нового уровня
         * - при полном рестарте игры
         */
        void clear() {
            mDense.clear();
            mSparse.clear();
            mFreeList.clear();
            mSparse.resize(1, kInvalidIndex); // оставляем одну ячейку под "0"
        }

    private:
        // Специальный индекс, означающий, что элемента нет в mDense
        static constexpr EntityId kInvalidIndex = std::numeric_limits<EntityId>::max();

        // Плотный вектор живых сущностей
        std::vector<Entity> mDense;

        // Разрежённый вектор: id → индекс в mDense, либо kInvalidIndex, если сущности с таким id сейчас нет
        std::vector<EntityId> mSparse{ 1, kInvalidIndex }; // 0-й id всегда "пустой"

        // Список свободных id (поиск по принципу LIFO — вектор быстрее очереди, у которой поиск FIFO)
        std::vector<EntityId> mFreeList;
    };

} // namespace core::ecs