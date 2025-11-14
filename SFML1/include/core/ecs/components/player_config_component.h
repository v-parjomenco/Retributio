#pragma once
#include "core/config/config_loader.h"

namespace core::ecs {

    /**
     * @brief Компонент конфигурации игрока (загружается из JSON).
     * Хранит копию core::config::PlayerConfig, загруженную из JSON через ConfigLoader.
     */
    struct PlayerConfigComponent {
        core::config::PlayerConfig config;
    };

} // namespace core::ecs