// ================================================================================================
// File: core/config/engine_settings.h
// Purpose: Runtime engine settings (VSync, frame limit, etc.), loaded from JSON.
// Used by: main/game loop (window setup), renderer bootstrap.
// Related headers: core/config/loader/engine_settings_loader.h
// ================================================================================================
#pragma once

#include <cstdint>

namespace core::config {

    /**
     * @brief Runtime-настройки движка.
     *
     * Здесь хранятся параметры, которые:
     *  - не относятся к конкретной игре;
     *  - могут быть переопределены без перекомпиляции через JSON.
     *
     * Источник истины:
     *  - дефолты ниже;
     *  - поверх них накрывается engine_settings.json.
     */
    struct EngineSettings {
        /// @brief Включить ли вертикальную синхронизацию.
        bool vsyncEnabled = true;

        /**
         * @brief Ограничение FPS через setFramerateLimit (0 — отключено).
         *
         * Важно:
         *  - если vsyncEnabled == true, логика окна может игнорировать frameLimit;
         *  - это именно user/dev-настройка, а не часть архитектуры движка.
         */
        std::uint32_t frameLimit = 0;
    };

} // namespace core::config