// ================================================================================================
// File: game/skyguard/ecs/systems/aircraft_control_system.h
// Purpose: Game-layer aircraft controls (turn/thrust bindings)
// Used by: Game loop (event forwarding), World/SystemManager
// ================================================================================================
#pragma once

#include <array>
#include <cmath>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Keyboard.hpp>

#include "core/ecs/components/movement_stats_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/components/velocity_component.h"
#include "core/ecs/system.h"
#include "core/ecs/world.h"
#include "core/utils/math_constants.h"

#include "game/skyguard/ecs/components/aircraft_control_component.h"
#include "game/skyguard/ecs/components/aircraft_control_bindings_component.h"

namespace game::skyguard::ecs {

    /**
     * @brief Игровая система управления кораблём (SkyGuard).
     *
     * Контракт:
     *  - turnLeft/turnRight           -> angular velocity (deg/s).
     *  - thrustForward/thrustBackward -> thrust along forward (cos/sin).
     *  - friction/maxSpeed применяются в game-layer.
     */
    class AircraftControlSystem final : public core::ecs::ISystem {
      public:
        AircraftControlSystem() = default;

        void resetState() noexcept {
            mKeyDown.fill(false);
        }

        void onKeyEvent(sf::Keyboard::Key code, bool pressed) noexcept {
            const auto index = tryKeyIndex(code);
            if (index == kInvalidIndex) {
                return;
            }
            mKeyDown[index] = pressed;
        }

        void update(core::ecs::World& world, float dt) noexcept override {
            if (dt == 0.f) {
                return;
            }

            auto view = world.view<AircraftControlBindingsComponent,
                                   core::ecs::MovementStatsComponent,
                                   core::ecs::TransformComponent,
                                   core::ecs::VelocityComponent,
                                   AircraftControlComponent>();

            view.each([this, dt](const AircraftControlBindingsComponent& controls,
                                 const core::ecs::MovementStatsComponent& stats,
                                 const core::ecs::TransformComponent& transform,
                                 core::ecs::VelocityComponent& velocity,
                                 const AircraftControlComponent& aircraft) {
                float turnDir = 0.f;
                if (isDown(controls.turnLeft)) {
                    turnDir -= 1.f;
                }
                if (isDown(controls.turnRight)) {
                    turnDir += 1.f;
                }
                velocity.angularDegreesPerSec = turnDir * aircraft.turnRateDegreesPerSec;

                float thrustDir = 0.f;
                if (isDown(controls.thrustForward)) {
                    thrustDir += 1.f;
                }
                if (isDown(controls.thrustBackward)) {
                    thrustDir -= 1.f;
                }

                if (thrustDir != 0.f && stats.acceleration > 0.f) {
                    const float rad = core::utils::degToRad(transform.rotationDegrees);
                    const sf::Vector2f forward{std::cos(rad), std::sin(rad)};
                    velocity.linear += forward * (stats.acceleration * thrustDir * dt);
                } else if (stats.friction > 0.f) {
                    const float vx = velocity.linear.x;
                    const float vy = velocity.linear.y;
                    const float speedSq = (vx * vx) + (vy * vy);
                    if (speedSq > 0.f) {
                        const float speed = std::sqrt(speedSq);
                        const float drop = stats.friction * dt;
                        const float newSpeed = (speed > drop) ? (speed - drop) : 0.f;
                        const float scale = (newSpeed > 0.f) ? (newSpeed / speed) : 0.f;
                        velocity.linear = {vx * scale, vy * scale};
                    }
                }

                if (stats.maxSpeed > 0.f) {
                    const float vx = velocity.linear.x;
                    const float vy = velocity.linear.y;
                    const float speedSq = (vx * vx) + (vy * vy);
                    const float maxSpeedSq = stats.maxSpeed * stats.maxSpeed;
                    if (speedSq > maxSpeedSq && speedSq > 0.f) {
                        const float speed = std::sqrt(speedSq);
                        const float scale = stats.maxSpeed / speed;
                        velocity.linear = {vx * scale, vy * scale};
                    }
                }
            });
        }

        void render(core::ecs::World&, sf::RenderWindow&) override {
        }

      private:
        static constexpr std::size_t kInvalidIndex = static_cast<std::size_t>(-1);

        std::array<bool, sf::Keyboard::KeyCount> mKeyDown{};

        [[nodiscard]] std::size_t tryKeyIndex(sf::Keyboard::Key key) const noexcept {
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

} // namespace game::skyguard::ecs