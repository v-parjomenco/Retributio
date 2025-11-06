#pragma once

#include <cstdint>
#include <functional>

namespace core::ecs {

    // Базовый "идентификатор сущности".
    // Сейчас это просто обёртка над uint32_t.
    // Позже сюда легко добавить "generation", если нужно будет защищаться
    // от висячих Entity после удаления (entity id + generation).
    struct Entity {
        std::uint32_t id{ 0 };

        // Удобно сравнивать как обычные числа
        bool operator==(const Entity& other) const noexcept { return id == other.id; }
        bool operator!=(const Entity& other) const noexcept { return id != other.id; }
        bool operator<(const Entity& other)  const noexcept { return id < other.id; }

        // Быстрая проверка "валидная ли сущность вообще"
        explicit operator bool() const noexcept { return id != 0; }
    };

    // Спец-значение "нет сущности"
    inline constexpr Entity NullEntity{ 0 };

} // namespace core::ecs

namespace std {
    template <>
    struct hash<core::ecs::Entity> {
        std::size_t operator()(const core::ecs::Entity& e) const noexcept {
            return std::hash<std::uint32_t>{}(e.id);
        }
    };
} // namespace std