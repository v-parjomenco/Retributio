#pragma once
#include "core/config/config_loader.h"

namespace core::ecs {

    /**
     * @brief Компонент конфигурации игрока (загружается из JSON).
     */
    struct PlayerConfigComponent {
        config::PlayerConfig config;
    };

} // namespace core::ecs