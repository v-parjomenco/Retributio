// ================================================================================================
// File: core/log/log_macros.h
// Purpose: Convenience macros for logging with file/line information.
// Notes:
//  - Wraps core::log::detail::logf_impl / panicf_impl with __FILE__/__LINE__.
//  - Provides LOG_TRACE/DEBUG/INFO/WARN/ERROR/CRITICAL/PANIC macros.
// ================================================================================================
#pragma once

#include "core/log/log_level.h"
#include "core/log/logging.h"
#include "core/log/log_categories.h"

namespace core::log {
    // Здесь намеренно нет кода — только подключение заголовков.
    // Макросы объявлены ниже вне namespace.
} // namespace core::log

#ifdef _DEBUG

    #define LOG_TRACE(category, formatStr, ...)                                          \
        do {                                                                             \
            if (::core::log::wouldLog(::core::log::Level::Trace, (category))) {          \
                ::core::log::detail::logf_impl(                                          \
                    ::core::log::Level::Trace,                                           \
                    (category),                                                          \
                    __FILE__,                                                            \
                    __LINE__,                                                            \
                    (formatStr) __VA_OPT__(,) __VA_ARGS__);                              \
            }                                                                            \
        } while (0)

    #define LOG_DEBUG(category, formatStr, ...)                                          \
        do {                                                                             \
            if (::core::log::wouldLog(::core::log::Level::Debug, (category))) {          \
                ::core::log::detail::logf_impl(                                          \
                    ::core::log::Level::Debug,                                           \
                    (category),                                                          \
                    __FILE__,                                                            \
                    __LINE__,                                                            \
                    (formatStr) __VA_OPT__(,) __VA_ARGS__);                              \
            }                                                                            \
        } while (0)

#else

    #define LOG_TRACE(category, formatStr, ...)                                          \
        do { (void)0; } while (0)

    #define LOG_DEBUG(category, formatStr, ...)                                          \
        do { (void)0; } while (0)

#endif // _DEBUG

#define LOG_INFO(category, formatStr, ...)                                               \
    do {                                                                                 \
        if (::core::log::wouldLog(::core::log::Level::Info, (category))) {               \
            ::core::log::detail::logf_impl(                                              \
                ::core::log::Level::Info,                                                \
                (category),                                                              \
                __FILE__,                                                                \
                __LINE__,                                                                \
                (formatStr) __VA_OPT__(,) __VA_ARGS__);                                  \
        }                                                                                \
    } while (0)

#define LOG_WARN(category, formatStr, ...)                                               \
    do {                                                                                 \
        if (::core::log::wouldLog(::core::log::Level::Warning, (category))) {            \
            ::core::log::detail::logf_impl(                                              \
                ::core::log::Level::Warning,                                             \
                (category),                                                              \
                __FILE__,                                                                \
                __LINE__,                                                                \
                (formatStr) __VA_OPT__(,) __VA_ARGS__);                                  \
        }                                                                                \
    } while (0)

#define LOG_ERROR(category, formatStr, ...)                                              \
    do {                                                                                 \
        if (::core::log::wouldLog(::core::log::Level::Error, (category))) {              \
            ::core::log::detail::logf_impl(                                              \
                ::core::log::Level::Error,                                               \
                (category),                                                              \
                __FILE__,                                                                \
                __LINE__,                                                                \
                (formatStr) __VA_OPT__(,) __VA_ARGS__);                                  \
        }                                                                                \
    } while (0)

#define LOG_CRITICAL(category, formatStr, ...)                                           \
    do {                                                                                 \
        if (::core::log::wouldLog(::core::log::Level::Critical, (category))) {           \
            ::core::log::detail::logf_impl(                                              \
                ::core::log::Level::Critical,                                            \
                (category),                                                              \
                __FILE__,                                                                \
                __LINE__,                                                                \
                (formatStr) __VA_OPT__(,) __VA_ARGS__);                                  \
        }                                                                                \
    } while (0)

#define LOG_PANIC(category, formatStr, ...)                                              \
    do {                                                                                 \
        ::core::log::detail::panicf_impl(                                                \
            (category),                                                                  \
            __FILE__,                                                                    \
            __LINE__,                                                                    \
            (formatStr) __VA_OPT__(,) __VA_ARGS__);                                      \
    } while (0)