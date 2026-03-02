// ================================================================================================
// File: core/runtime/entry/logging_lifetime.h
// Purpose: RAII wrapper for core::log lifetime — guarantees shutdown on all exit paths.
// Used by: Entry point mains (main_skyguard, main_spatial_harness, main_render_stress).
// Related: core/log/logging.h
// Notes:
//  - Non-copyable, non-movable: exactly one live instance must exist at a time.
//    Two live instances = double init + double shutdown = undefined behaviour.
//  - Destructor is noexcept: shutdown() is called inside try/catch — exceptions swallowed.
//  - In Debug: asserts logger is not already active before construction.
//    This catches violation of the entry-layer policy (LOG_* called before LoggingLifetime).
//    The underlying logger handles the violation gracefully via lazy-init, so no UB in Release.
// ================================================================================================
#pragma once

#include <type_traits>

#include "core/log/logging.h"

namespace core::runtime::entry {

    class LoggingLifetime {
    public:
        /// Инициализирует логгер с переданным конфигом.
        /// В Debug: assert срабатывает если логгер уже активен (нарушение entry-layer политики).
        explicit LoggingLifetime(core::log::Config cfg);

        /// Завершает работу логгера. noexcept: исключения из shutdown() поглощаются.
        ~LoggingLifetime() noexcept;

        LoggingLifetime(const LoggingLifetime&)            = delete;
        LoggingLifetime& operator=(const LoggingLifetime&) = delete;
        LoggingLifetime(LoggingLifetime&&)                 = delete;
        LoggingLifetime& operator=(LoggingLifetime&&)      = delete;
    };

} // namespace core::runtime::entry

// Компиляционные гарантии владения — нарушение = ошибка сборки.
static_assert(!std::is_copy_constructible_v<core::runtime::entry::LoggingLifetime>);
static_assert(!std::is_copy_assignable_v<core::runtime::entry::LoggingLifetime>);
static_assert(!std::is_move_constructible_v<core::runtime::entry::LoggingLifetime>);
static_assert(!std::is_move_assignable_v<core::runtime::entry::LoggingLifetime>);
static_assert(std::is_nothrow_destructible_v<core::runtime::entry::LoggingLifetime>);
