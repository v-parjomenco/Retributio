// ================================================================================================
// File: core/ecs/systems/input_system.h
// Purpose: Map global keyboard state to per-entity target velocity
// Used by: Game loop (event forwarding), World/SystemManager
// Related headers: keyboard_control_component.h, movement_stats_component.h, velocity_component.h
// ================================================================================================
#pragma once

#include <array>
#include <cmath>

#include <SFML/Window/Keyboard.hpp>

#include "core/ecs/components/keyboard_control_component.h"
#include "core/ecs/components/movement_stats_component.h"
#include "core/ecs/components/velocity_component.h"
#include "core/ecs/system.h"
#include "core/ecs/world.h"

namespace core::ecs {

    /**
     * @brief Система ввода: преобразует глобальное состояние клавиатуры
     * в целевую скорость для сущностей.
     *
     * Поток данных:
     *  - Game пересылает события KeyPressed/KeyReleased в onKeyEvent(...);
     *  - InputSystem хранит текущее состояние всех клавиш (bool-массив);
     *  - update():
     *      * по KeyboardControlComponent определяет, какие клавиши считаются up/down/left/right
     *      * вычисляет нормализованный вектор направления
     *      * множит на MovementStatsComponent::maxSpeed и пишет в VelocityComponent::linear.
     *
     * Таким образом:
     *  - раскладка управления и параметры движения задаются через компоненты (data-driven);
     *  - система остаётся универсальной для любых игр (SkyGuard, платформер, 4X и т.д.).
     */
    class InputSystem final : public ISystem {
      public:
        InputSystem() {
            mKeyDown.fill(false);
        }

        /// @brief Обновить состояние одной клавиши.
        /// @param code код клавиши SFML
        /// @param pressed true, если нажата; false, если отпущена
        void onKeyEvent(sf::Keyboard::Key code, bool pressed) {
            if (code == sf::Keyboard::Key::Unknown) {
                return;
            }

            const auto idx = static_cast<int>(code);
            if (idx < 0) {
                return;
            }

            const auto index = static_cast<std::size_t>(idx);
            if (index >= mKeyDown.size()) {
                return;
            }

            mKeyDown[index] = pressed;
        }

        void update(World& world, float) override {
            auto& controls  = world.storage<KeyboardControlComponent>();
            auto& stats     = world.storage<MovementStatsComponent>();
            auto& velos     = world.storage<VelocityComponent>();

            for (auto& [entity, cc] : controls) {
                auto* st = stats.get(entity);
                auto* v = velos.get(entity);
                if (!st || !v) {
                    continue;
                }

                // Вычисляем направление по раскладке сущности и глобальному состоянию клавиш
                float dirX = 0.f;
                float dirY = 0.f;

                if (isDown(cc.left)) {
                    dirX -= 1.f;
                }
                if (isDown(cc.right)) {
                    dirX += 1.f;
                }
                if (isDown(cc.up)) {
                    dirY -= 1.f;
                }
                if (isDown(cc.down)) {
                    dirY += 1.f;
                }

                // Нормализация по диагонали, чтобы вправо+вверх не было быстрее, чем просто вправо
                if (dirX != 0.f || dirY != 0.f) {
                    const float lenSq = dirX * dirX + dirY * dirY;
                    const float invLen = 1.f / std::sqrt(lenSq);
                    dirX *= invLen;
                    dirY *= invLen;

                    v->linear = {dirX * st->maxSpeed, dirY * st->maxSpeed};
                } else {
                    v->linear = {0.f, 0.f};
                }
            }
        }

        void render(World&, sf::RenderWindow&) override {
            // Система ввода ничего не рисует.
        }

      private:
        // Используем KeyCount из SFML 3.0.2, чтобы не держать магическое число.
        std::array<bool, sf::Keyboard::KeyCount> mKeyDown;

        bool isDown(sf::Keyboard::Key key) const {
            const auto idx = static_cast<int>(key);
            if (idx < 0) {
                return false;
            }

            const auto index = static_cast<std::size_t>(idx);
            if (index >= mKeyDown.size()) {
                return false;
            }

            return mKeyDown[index];
        }
    };

} // namespace core::ecs