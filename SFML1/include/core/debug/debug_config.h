// ================================================================================================
// File: core/debug/debug_config.h
// Purpose: Compile-time debug configuration (overlay, exit UX, debug hotkeys).
// Notes:
//  - Эти флаги и хоткеи предназначены для разработчиков, а не для игроков.
//  - Gameplay-контролы (WASD, действия, биндинги) настраиваются через JSON.
//  - В Release-сборке overlay и debug-пауза по умолчанию отключены.
// ================================================================================================
#pragma once

#include <SFML/Window/Keyboard.hpp>

namespace core::config {

    // --------------------------------------------------------------------------------------------
    // Debug overlay (DebugOverlaySystem)
    // --------------------------------------------------------------------------------------------

#ifdef _DEBUG
    /// Показывать ли FPS overlay по умолчанию (в Debug обычно хотим видеть его сразу).
    inline constexpr bool SHOW_FPS_OVERLAY = true;
#else
    /// В Release overlay по умолчанию скрыт (может быть включён через debug-сборки/флаги).
    inline constexpr bool SHOW_FPS_OVERLAY = false;
#endif

    /**
     * @brief Hotkey для переключения debug overlay.
     *
     * Это dev-only хоткей:
     *  - не документируется в пользовательском мануале;
     *  - относится к инфраструктуре отладки, а не к управлению игроком;
     *  - может быть расширен набором других debug-hotkeys (консоль, профайлер и т.п.).
     *
     * Gameplay-хоткеи и биндинги должны жить в data-driven конфигурации (JSON).
     */
    inline constexpr sf::Keyboard::Key HOTKEY_TOGGLE_OVERLAY = sf::Keyboard::Key::F3;

    // --------------------------------------------------------------------------------------------
    // Debug UX при завершении приложения
    // --------------------------------------------------------------------------------------------

    /**
     * @brief Включать ли "паузы" при завершении приложения (holdOnExit).
     *
     * Используется только на верхнем уровне (main):
     *  - в Debug-сборке даёт время прочитать сообщения/вывод;
     *  - в Release всегда false, чтобы не мешать игроку нормально выходить из игры.
     */
#ifdef _DEBUG
    inline constexpr bool DEBUG_HOLD_ON_EXIT = true;
#else
    inline constexpr bool DEBUG_HOLD_ON_EXIT = false;
#endif

} // namespace core::config