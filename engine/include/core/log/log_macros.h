// ================================================================================================
// File: core/log/log_macros.h
// Purpose: Convenience macros for logging with file/line information.
// Notes:
//  - Wraps core::log::detail::logf_impl / panicf_impl with __FILE__/__LINE__.
//  - Provides LOG_TRACE/DEBUG/INFO/WARN/ERROR/CRITICAL/PANIC macros.
//  - category вычисляется ровно один раз за вызов макроса.
//  - Форматные строки проверяются в compile-time через std::format_string.
// ================================================================================================
#pragma once

#include "core/log/log_categories.h"
#include "core/log/log_level.h"
#include "core/log/logging.h"

// ------------------------------------------------------------------------------------------------
// Внутренний макрос реализации — все LOG_* уровни делегируют сюда.
// Вычисляет category ровно один раз. Гейтит через wouldLog() до форматирования.
// ------------------------------------------------------------------------------------------------
#define CORE_LOG_IMPL_(lvl, category, fmt, ...)                                              \
    do {                                                                                     \
        const auto& coreLogCat_ = (category);                                                \
        if (::core::log::wouldLog((lvl), coreLogCat_)) {                                     \
            ::core::log::detail::logf_impl(                                                  \
                (lvl),                                                                       \
                coreLogCat_,                                                                 \
                __FILE__,                                                                    \
                __LINE__,                                                                    \
                (fmt) __VA_OPT__(,) __VA_ARGS__);                                            \
        }                                                                                    \
    } while (0)

// ------------------------------------------------------------------------------------------------
// Debug-only уровни: вырезаются в Release.
// ------------------------------------------------------------------------------------------------

#ifdef _DEBUG

    #define LOG_TRACE(category, fmt, ...)                                                    \
        CORE_LOG_IMPL_(::core::log::Level::Trace, category, fmt __VA_OPT__(,) __VA_ARGS__)

    #define LOG_DEBUG(category, fmt, ...)                                                    \
        CORE_LOG_IMPL_(::core::log::Level::Debug, category, fmt __VA_OPT__(,) __VA_ARGS__)

#else

    #define LOG_TRACE(category, fmt, ...)  do { (void)0; } while (0)
    #define LOG_DEBUG(category, fmt, ...)  do { (void)0; } while (0)

#endif // _DEBUG

// ------------------------------------------------------------------------------------------------
// Всегда активные уровни (Info — Critical).
// ------------------------------------------------------------------------------------------------

#define LOG_INFO(category, fmt, ...)                                                         \
    CORE_LOG_IMPL_(::core::log::Level::Info, category, fmt __VA_OPT__(,) __VA_ARGS__)

#define LOG_WARN(category, fmt, ...)                                                         \
    CORE_LOG_IMPL_(::core::log::Level::Warning, category, fmt __VA_OPT__(,) __VA_ARGS__)

#define LOG_ERROR(category, fmt, ...)                                                        \
    CORE_LOG_IMPL_(::core::log::Level::Error, category, fmt __VA_OPT__(,) __VA_ARGS__)

#define LOG_CRITICAL(category, fmt, ...)                                                     \
    CORE_LOG_IMPL_(::core::log::Level::Critical, category, fmt __VA_OPT__(,) __VA_ARGS__)

// ------------------------------------------------------------------------------------------------
// PANIC: всегда активен, без гейта по уровню (безусловный), [[noreturn]].
// ------------------------------------------------------------------------------------------------

#define LOG_PANIC(category, fmt, ...)                                                        \
    do {                                                                                     \
        ::core::log::detail::panicf_impl(                                                    \
            (category),                                                                      \
            __FILE__,                                                                        \
            __LINE__,                                                                        \
            (fmt) __VA_OPT__(,) __VA_ARGS__);                                                \
    } while (0)
