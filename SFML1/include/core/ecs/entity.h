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
    inline constexpr Entity NullEntity{entt::null};

    /**
     * @brief Проверка, является ли сущность "пустой" (невалидной).
     */
    [[nodiscard]] constexpr bool isNull(const Entity entity) noexcept {
        return entity == NullEntity;
    }

    /**
     * @brief Преобразование Entity в uint32_t для логирования/отладки.
     *
     * ВАЖНО:
     *  - Это НЕ persistent ID (для сейвов нужен StableID сервис + маппинг).
     *  - Это просто числовое представление handle'а (включая generation биты).
     */
    [[nodiscard]] constexpr std::uint32_t toUint(const Entity entity) noexcept {
        using underlying_type = std::underlying_type_t<Entity>;
        static_assert(sizeof(underlying_type) <= sizeof(std::uint32_t),
                      "core::ecs::Entity underlying type must fit into uint32_t");

        return static_cast<std::uint32_t>(entt::to_integral(entity));
    }

} // namespace core::ecs

// Хеш-функция для использования Entity в std::unordered_map / std::unordered_set.
//
// Примечание:
//  - entt::entity по сути компактный enum-handle; хешируем по integral представлению.
namespace std {
    template <>
    struct hash<core::ecs::Entity> {
        std::size_t operator()(const core::ecs::Entity entity) const noexcept {
            using underlying_type = std::underlying_type_t<core::ecs::Entity>;
            const auto value = static_cast<underlying_type>(entt::to_integral(entity));
            return std::hash<underlying_type>{}(value);
        }
    };
} // namespace std
