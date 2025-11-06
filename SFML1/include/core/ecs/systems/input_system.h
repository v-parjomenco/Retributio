#pragma once

#include <array>
#include <cassert>
#include "core/ecs/system.h"
#include "core/ecs/world.h"
#include "core/ecs/components/keyboard_control_component.h"
#include "core/ecs/components/movement_stats_component.h"
#include "core/ecs/components/velocity_component.h"

namespace core::ecs {

    /**
     * @brief Система ввода: преобразует нажатые клавиши в целевую скорость сущностей.
     * - Game пересылает KeyPressed/KeyReleased в onKeyEvent(...)
     * - update() вычисляет вектор направления по нажатым клавишам и множит на maxSpeed
     */
    class InputSystem final : public ISystem {
    public:
        InputSystem() {
            keyDown.fill(false);
        }

        void onKeyEvent(sf::Keyboard::Key code, bool pressed) {
            if (code == sf::Keyboard::Key::Unknown)
                return;
            const int idx = static_cast<int>(code);
            if (idx < 0 || idx >= static_cast<int>(keyDown.size()))
                return;
            keyDown[static_cast<size_t>(idx)] = pressed;
        }

        void update(World& world, float) override {
            auto& controls = world.storage<KeyboardControlComponent>();
            auto& stats    = world.storage<MovementStatsComponent>();
            auto& velos    = world.storage<VelocityComponent>();

            for (auto& [entity, cc] : controls) {
                auto* st = stats.get(entity);
                auto* v  = velos.get(entity);
                if (!st || !v) continue;

                // Вычисляем направление по раскладке сущности и глобальному состоянию клавиш
                float dirX = 0.f, dirY = 0.f;
                if (isDown(cc.left))  dirX -= 1.f;
                if (isDown(cc.right)) dirX += 1.f;
                if (isDown(cc.up))    dirY -= 1.f;
                if (isDown(cc.down))  dirY += 1.f;

                // Нормализация по диагонали (чтобы вправо+вверх не было быстрее)
                if (dirX != 0.f || dirY != 0.f) {
                    const float invLen = 1.f / std::sqrt(dirX * dirX + dirY * dirY);
                    dirX *= invLen;
                    dirY *= invLen;
                    v->linear = { dirX * st->maxSpeed, dirY * st->maxSpeed };
                } else {
                    v->linear = { 0.f, 0.f };
                }
            }
        }

        void render(World&, sf::RenderWindow&) override {}

    private:
        std::array<bool, 512> keyDown; // запас по размеру

        bool isDown(sf::Keyboard::Key k) const {
            const int idx = static_cast<int>(k);
            if (idx < 0) return false;
            if (idx >= static_cast<int>(keyDown.size())) return false;
            return keyDown[static_cast<size_t>(idx)];
        }
    };

} // namespace core::ecs