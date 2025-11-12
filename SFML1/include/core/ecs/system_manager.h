// =====================================================
// File: core/ecs/system_manager.h
// Purpose: Owns and sequences systems (update/render)
// Used by: World
// Related headers: system.h
// =====================================================
#pragma once

#include <memory>
#include <type_traits>
#include <vector>

#include <SFML/Graphics.hpp>

#include "core/ecs/system.h"

namespace core::ecs {

    class World; // вперёд

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

        // Запрещаем копирование (из-за unique_ptr внутри)
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
            auto ptr = std::make_unique<T>(std::forward<Args>(args)...);
            T& ref = *ptr;
            mSystems.push_back(std::move(ptr));
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
        std::vector<std::unique_ptr<ISystem>> mSystems;
    };

} // namespace core::ecs