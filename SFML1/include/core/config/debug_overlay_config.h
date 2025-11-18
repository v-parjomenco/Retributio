// ================================================================================================
// File: core/config/debug_overlay_config.h
// Purpose: High-level config loader for on-screen debug overlay (FPS, timings, etc.)
// Used by: DebugOverlaySystem
// Related headers: core/utils/json/json_utils.h, core/utils/json/json_validator.h,
//                  core/utils/file_loader.h
// ================================================================================================
#pragma once

#include <string>

#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>

namespace core::config {

    // Высокоуровневая конфигурация для debug overlay.
    // Определяет позицию, размер шрифта, его цвет, включение/отключение.
    struct DebugOverlayConfig {
        bool enabled = true;                // показывать ли вообще overlay
        sf::Vector2f position{10.f, 10.f};  // позиция текста в мировых координатах / пикселях
        unsigned int characterSize = 35;    // размер шрифта
        sf::Color color{255, 0, 0, 255};    // цвет текста по умолчанию (красный)
    };

    // Загружает JSON из файла и парсит данные в DebugOverlayConfig.
    //
    // Возможное поведение:
    //  - Если файла нет / не читается           -> logDebug(...) + дефолт из DebugOverlayConfig.
    //  - Если JSON некорректен или невалиден    -> logDebug(...) + дефолт из DebugOverlayConfig.
    //  - Если отдельные поля отсутствуют        -> дефолт из DebugOverlayConfig.
    //
    DebugOverlayConfig loadDebugOverlayConfig(const std::string& path);

} // namespace core::config