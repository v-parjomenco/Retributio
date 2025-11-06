#pragma once
#include "core/config_loader.h"

namespace core::ecs {

    /**
     * @brief Компонент конфигурации игрока (загружается из JSON).
     */
    struct PlayerConfigComponent {
        core::PlayerConfig config;
    };

} // namespace core::ecs