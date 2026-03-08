// ================================================================================================
// File: core/runtime/entry/terminate_handler.h
// Purpose: Installs a std::terminate handler with crash logging and clean process exit.
// Used by: Entry point mains (main_atrapacielos, main_stress_spatial, main_stress_render).
// Related: core/log/logging.h, core/log/log_macros.h
// Notes:
//  - Must be called as early as possible — before any other initialization.
//  - If the logger is not yet active, the handler falls back to stderr and
//    OutputDebugStringW (Windows) before terminating via std::abort().
//  - If the logger is active, calls LOG_PANIC which guarantees flush and panic_sink exit.
//  - Recursive std::terminate calls are detected via an atomic flag and abort immediately.
// ================================================================================================
#pragma once

namespace core::runtime::entry {

    /// Устанавливает std::terminate handler. Безопасно вызывать до инициализации логгера.
    /// Повторные вызовы безопасны: каждый вызов перезаписывает handler (std::set_terminate).
    void installTerminateHandler() noexcept;

} // namespace core::runtime::entry
