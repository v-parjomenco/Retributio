// ================================================================================================
// File: core/runtime/entry/user_dialog.h
// Purpose: User-facing entry-layer UX: native error dialogs and debug exit pause.
// Used by: main_skyguard.cpp (game entry point with GUI and Debug UX).
// Related: core/debug/debug_config.h (DEBUG_HOLD_ON_EXIT), core/log/detail/utf8_to_wide.h
// Notes:
//  - showError: Windows — MessageBoxW (UTF-8 → UTF-16 via core::log::detail::utf8ToWide);
//               Unix/macOS — stderr. Safe to call before logger initialization.
//  - holdOnExitIfEnabled: no-op in Release and Profile (DEBUG_HOLD_ON_EXIT == false).
//    Intended for game/GUI entry points only — do not call from console tools (harness).
//  - All functions are noexcept.
// ================================================================================================
#pragma once

#include <string_view>

namespace core::runtime::entry {

    /// Показывает нативный диалог с сообщением об ошибке.
    /// Windows: MessageBoxW. Unix/macOS: вывод в stderr.
    /// Безопасно вызывать до инициализации логгера. noexcept.
    void showError(std::string_view text) noexcept;

    /// Приостанавливает завершение процесса в Debug-сборках (core::debug::DEBUG_HOLD_ON_EXIT).
    /// No-op в Release и Profile. Предназначен только для GUI entry points. noexcept.
    void holdOnExitIfEnabled() noexcept;

} // namespace core::runtime::entry
