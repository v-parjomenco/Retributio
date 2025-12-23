// ================================================================================================
// File: core/ecs/system_manager.h
// Purpose: Owns and orchestrates ECS systems.
// Used by: core/ecs/world.h
// ================================================================================================
#pragma once

#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/ecs/system.h"

namespace core::ecs {

    class World;

    /**
     * @brief Простой менеджер систем.
     *
     * Хранит все системы в одном месте и умеет:
     *  - вызывать update у всех
     *  - вызывать render у всех
     *  - добавлять новые системы
     */
    class SystemManager {
      public:
        SystemManager() = default;
        ~SystemManager() = default;

        // SystemManager — владелец систем, и его копирование запрещено по архитектуре.
        SystemManager(const SystemManager&) = delete;
        SystemManager& operator=(const SystemManager&) = delete;
        // Разрешаем перемещение
        SystemManager(SystemManager&&) noexcept = default;
        SystemManager& operator=(SystemManager&&) noexcept = default;

        /**
         * @brief Добавить систему любого типа.
         * @return ссылка на созданную систему, чтобы можно было её дальше настраивать.
         */
        template <typename T, typename... Args>
        T& addSystem(Args&&... args) {
            static_assert(std::is_base_of_v<ISystem, T>, "T must derive from ISystem");

            auto sys = std::make_unique<T>(std::forward<Args>(args)...);
            T& ref = *sys;
            mSystems.push_back(std::move(sys));
            return ref;
        }

        /**
         * @brief Вызвать update у всех систем.
         */
        void updateAll(World& world, float dt) {
            for (auto& sys : mSystems) {
                sys->update(world, dt);
            }
        }

        /**
         * @brief Вызвать render у всех систем.
         */
        void renderAll(World& world, sf::RenderWindow& window) {
            for (auto& sys : mSystems) {
                sys->render(world, window);
            }
        }

      private:
        /// @note Инвариант времени жизни:
        ///       системы хранятся в куче (unique_ptr), поэтому указатели/ссылки,
        ///       полученные из addSystem(), остаются валидны при реаллокациях vector.
        ///       Не переводить хранение на by-value контейнеры без пересмотра кэширования.
        std::vector<std::unique_ptr<ISystem>> mSystems;
    };

} // namespace core::ecs