// ================================================================================================
// File: core/config.h
// Purpose: Engine-wide compile-time configuration (timing, debug toggles).
// Notes:
//  - Does not contain game values (SkyGuard windows, Player, paths to assets, etc.).
//  - The source of truth for gameplay settings — JSON + enum defaults in properties.
//  - VSync / frame limit and other runtime settings are now read from engine_settings.json.
// ================================================================================================
#pragma once

#include <SFML/System/Time.hpp>
#include <SFML/Window/Keyboard.hpp>

namespace core::config {

    // --------------------------------------------------------------------------------------------
    // Фиксированный шаг логики движка (используется в игровом цикле)
    // --------------------------------------------------------------------------------------------
    inline const sf::Time FIXED_TIME_STEP = sf::seconds(1.f / 60.f);

    // --------------------------------------------------------------------------------------------
    // Переключатели для debug overlay (DebugOverlaySystem)
    // --------------------------------------------------------------------------------------------
#ifdef _DEBUG
    // Показывать ли оверлей по умолчанию: debug - true, release - false
    inline constexpr bool SHOW_FPS_OVERLAY = true;
#else
    inline constexpr bool SHOW_FPS_OVERLAY = false;
#endif

    // Хоткей для тоггла (оставляем и в релизе: неактивно, если оверлей не добавлен/выключен)
    inline constexpr sf::Keyboard::Key HOTKEY_TOGGLE_OVERLAY = sf::Keyboard::Key::F3;

    // --------------------------------------------------------------------------------------------
    // Отладка завершения приложения
    // --------------------------------------------------------------------------------------------
    // Показывать ли сообщение при завершении программы (для отладки)
    inline constexpr bool DEBUG_HOLD_ON_EXIT = true;

} // namespace core::config