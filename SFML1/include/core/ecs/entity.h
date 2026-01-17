// ================================================================================================
// File: core/ecs/entity.h
// Purpose: Canonical entity handle type for ECS (EnTT backend)
// Used by: core/ecs/world.h, all ECS systems, game layer entity references
// Related headers: world.h, registry.h, adapters/entt/entt_registry.hpp
// ================================================================================================
#pragma once

#include <cstdint>
#include <functional>
#include <type_traits>

#include "adapters/entt/entt_registry.hpp"

namespace core::ecs {

    /**
     * @brief Дескриптор сущности в ECS (прямой алиас entt::entity).
     *
     * Изменения от старой реализации:
     *  - Больше нет собственного поля `uint32_t id`;
     *  - Entity теперь это `entt::entity` (enum class с встроенной generation);
     *  - Все операции с числовым представлением идут через helper-функции.
     *
     * Гарантии совместимости:
     *  - Старый код вида `entity.id` НЕ компилируется (это хорошо - ловим ошибки);
     *  - Вместо этого используем `toUint(entity)` для логирования/отладки;
     *  - Сравнения `entity == otherEntity` работают как раньше (operator==).
     *
     * EnTT детали:
     *  - entt::entity включает в себя generation counter (защита от use-after-free);
     *  - entt::null — специальное значение "пустой сущности".
     */
    using Entity = entt::entity;

    /**
     * @brief Специальное значение "пустая сущность".
     *
     * Заменяет старый `Entity{0}` или проверки `entity.id == 0`.
     * Используется для инициализации и проверки на "нет сущности".
     */
    inline constexpr Entity NullEntity = entt::null;

    /**
     * @brief Проверка, является ли сущность "пустой" (невалидной).
     *
     * @param entity Дескриптор сущности для проверки
     * @return true если entity == entt::null
     *
     * Пример использования:
     *   Entity e = world.createEntity();
     *   if (!isNull(e)) {
     *       // сущность валидна
     *   }
     */
    [[nodiscard]] constexpr bool isNull(Entity entity) noexcept {
        return entity == NullEntity;
    }

    /**
     * @brief Преобразование Entity в uint32_t для логирования/отладки.
     *
     * ВАЖНО:
     *  - Это НЕ "индекс" в массиве компонентов (EnTT управляет этим внутри);
     *  - Это удобное числовое представление для человека (логи, UI, отладка);
     *  - НЕ используйте это значение как persistent ID для сохранений/загрузок между запусками:
     *    для сейвов нужен отдельный стабильный идентификатор и маппинг.
     *  - Для доступа к компонентам используйте World::getComponent<T>(entity).
     *
     * Замена старого кода:
     *   ДО:  entity.id
     *   ПОСЛЕ: toUint(entity)
     *
     * @param entity Дескриптор сущности
     * @return uint32_t числовое представление (включает generation)
     */
    [[nodiscard]] constexpr std::uint32_t toUint(Entity entity) noexcept {
        // entt::to_integral возвращает числовое представление дескриптора (underlying value).
        // Для null entity возвращаем явно 0xFFFFFFFF, чтобы в логах это не сливалось с обычными ID.
        if (entity == entt::null) {
            return 0xFFFFFFFF; // Явный маркер "null entity"
        }
        return static_cast<std::uint32_t>(entt::to_integral(entity));
    }

} // namespace core::ecs

// Хеш-функция для использования Entity в std::unordered_map / std::unordered_set.
//
// Технически entt::entity уже hashable через entt::to_integral, но явная
// специализация std::hash гарантирует совместимость со стандартными контейнерами.
namespace std {
    template <>
    struct hash<core::ecs::Entity> {
        std::size_t operator()(core::ecs::Entity entity) const noexcept {
            // Используем underlying type для хеша (uint32_t с generation битами)
            using underlying_type = std::underlying_type_t<core::ecs::Entity>;
            const auto value = static_cast<underlying_type>(entt::to_integral(entity));
            return std::hash<underlying_type>{}(value);
        }
    };
} // namespace std