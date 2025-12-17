// ================================================================================================
// File: core/ecs/systems/input_system.h
// Purpose: Map global keyboard state to per-entity target velocity (EnTT backend)
// Used by: Game loop (event forwarding), World/SystemManager
// Related headers: keyboard_control_component.h, movement_stats_component.h, velocity_component.h
// ================================================================================================
#pragma once

#include <array>

#include <SFML/Window/Keyboard.hpp>

#include "core/ecs/components/keyboard_control_component.h"
#include "core/ecs/components/movement_stats_component.h"
#include "core/ecs/components/velocity_component.h"
#include "core/ecs/system.h"
#include "core/ecs/world.h"

namespace core::ecs {

    /**
     * @brief Система ввода: преобразует глобальное состояние клавиатуры
     *        в целевую скорость для сущностей.
     *
     * Поток данных:
     *  - Game пересылает события KeyPressed/KeyReleased в onKeyEvent(...);
     *  - InputSystem хранит текущее состояние всех клавиш (bool-массив);
     *  - update():
     *      * берёт раскладку из KeyboardControlComponent
     *      * вычисляет направление (-1/0/1 по осям)
     *      * нормализует диагональ (чтобы не было быстрее)
     *      * пишет скорость в VelocityComponent::linear,
     *        используя maxSpeed из MovementStatsComponent.
     *
     * Замечания:
     *  - Система не читает события напрямую из SFML (чтобы не тащить Event в ECS);
     *  - Система не знает ничего о конкретной игре: раскладка и параметры в компонентах.
     */
    class InputSystem final : public ISystem {
      public:
        InputSystem() = default;

        /// @brief Обновить состояние одной клавиши.
        void onKeyEvent(sf::Keyboard::Key code, bool pressed) noexcept {
            const auto index = tryKeyIndex(code);
            if (index == kInvalidIndex) {
                return;
            }
            mKeyDown[index] = pressed;
        }

        void update(World& world, float) override {
            auto view =
                world.view<KeyboardControlComponent, MovementStatsComponent, VelocityComponent>();

            view.each([this](const KeyboardControlComponent& controls,
                             const MovementStatsComponent& stats, VelocityComponent& velocity) {
                float dirX = 0.f;
                float dirY = 0.f;

                if (isDown(controls.left)) {
                    dirX -= 1.f;
                }
                if (isDown(controls.right)) {
                    dirX += 1.f;
                }
                if (isDown(controls.up)) {
                    dirY -= 1.f;
                }
                if (isDown(controls.down)) {
                    dirY += 1.f;
                }

                if (dirX != 0.f && dirY != 0.f) {
                    dirX *= kInvSqrt2;
                    dirY *= kInvSqrt2;
                }

                velocity.linear = {dirX * stats.maxSpeed, dirY * stats.maxSpeed};
            });
        }

        void render(World&, sf::RenderWindow&) override {
            // Система ввода ничего не рисует.
        }

      private:
        static constexpr std::size_t kInvalidIndex = static_cast<std::size_t>(-1);
        static constexpr float kInvSqrt2 = 0.70710678118f; // 1/sqrt(2)

        // KeyCount из SFML — размер массива (избавляет от магических чисел).
        std::array<bool, sf::Keyboard::KeyCount> mKeyDown{};

        [[nodiscard]] std::size_t tryKeyIndex(sf::Keyboard::Key key) const noexcept {
            // Unknown в SFML может быть отрицательным значением.
            if (key == sf::Keyboard::Key::Unknown) {
                return kInvalidIndex;
            }

            const int idx = static_cast<int>(key);
            if (idx < 0 || idx >= static_cast<int>(mKeyDown.size())) {
                return kInvalidIndex;
            }

            return static_cast<std::size_t>(idx);
        }

        [[nodiscard]] bool isDown(sf::Keyboard::Key key) const noexcept {
            const auto index = tryKeyIndex(key);
            return (index != kInvalidIndex) ? mKeyDown[index] : false;
        }
    };

} // namespace core::ecs
