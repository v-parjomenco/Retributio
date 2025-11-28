// ================================================================================================
// File: core/ecs/entity.h
// Purpose: Lightweight entity identifier wrapper (uint32_t)
// Used by: ComponentStorage<T>, EntityManager, World
// Related headers: entity_manager.h
// ================================================================================================
#pragma once

#include <cstdint>
#include <functional>

namespace core::ecs {

    /**
     * @brief Базовый "идентификатор сущности".
     *
     * Сейчас это тонкая обёртка над uint32_t без каких-либо скрытых аллокаций.
     * Можно безболезненно расширить до схемы "id + generation",
     * если потребуется дополнительная защита от висячих Entity после удаления.
     */
    struct Entity {
        std::uint32_t id{0};

        // Удобно сравнивать как обычные числа
        constexpr bool operator==(const Entity& other) const noexcept {
            return id == other.id;
        }
        constexpr bool operator!=(const Entity& other) const noexcept {
            return id != other.id;
        }
        constexpr bool operator<(const Entity& other) const noexcept {
            return id < other.id;
        }

        // Быстрая проверка "валидная ли сущность вообще"
        [[nodiscard]] explicit constexpr operator bool() const noexcept {
            return id != 0;
        }
    };

    // Спец-значение "нет сущности"
    inline constexpr Entity NullEntity{0};

} // namespace core::ecs

namespace std {
    template <> struct hash<core::ecs::Entity> {
        std::size_t operator()(const core::ecs::Entity& e) const noexcept {
            return std::hash<std::uint32_t>{}(e.id);
        }
    };
} // namespace std
