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

// ------------------------------------------------------------------------------------------------
// Основные макросы логирования:
//
//   LOG_INFO(core::log::cat::Resources, "Loaded {} textures", count);
//   LOG_ERROR(core::log::cat::Config,   "Failed to parse config '{}'", path);
//
// В Debug-сборках LOG_TRACE / LOG_DEBUG активны,
// в Release они вырезаются на уровне препроцессора.
// ------------------------------------------------------------------------------------------------

#ifdef _DEBUG

    #define LOG_TRACE(category, formatStr, ...)                                          \
        do {                                                                             \
            ::core::log::detail::logf_impl(                                              \
                ::core::log::Level::Trace,                                               \
                (category),                                                              \
                __FILE__,                                                                \
                __LINE__,                                                                \
                (formatStr) __VA_OPT__(,) __VA_ARGS__);                                  \
        } while (0)

    #define LOG_DEBUG(category, formatStr, ...)                                          \
        do {                                                                             \
            ::core::log::detail::logf_impl(                                              \
                ::core::log::Level::Debug,                                               \
                (category),                                                              \
                __FILE__,                                                                \
                __LINE__,                                                                \
                (formatStr) __VA_OPT__(,) __VA_ARGS__);                                  \
        } while (0)

#else
    // В Release-сборке Trace/Debug полностью вырезаются
    #define LOG_TRACE(category, formatStr, ...)                                          \
        do { (void)0; } while (0)

    #define LOG_DEBUG(category, formatStr, ...)                                          \
        do { (void)0; } while (0)

#endif // _DEBUG

#define LOG_INFO(category, formatStr, ...)                                               \
    do {                                                                                 \
        ::core::log::detail::logf_impl(                                                  \
            ::core::log::Level::Info,                                                    \
            (category),                                                                  \
            __FILE__,                                                                    \
            __LINE__,                                                                    \
            (formatStr) __VA_OPT__(,) __VA_ARGS__);                                      \
    } while (0)

#define LOG_WARN(category, formatStr, ...)                                               \
    do {                                                                                 \
        ::core::log::detail::logf_impl(                                                  \
            ::core::log::Level::Warning,                                                 \
            (category),                                                                  \
            __FILE__,                                                                    \
            __LINE__,                                                                    \
            (formatStr) __VA_OPT__(,) __VA_ARGS__);                                      \
    } while (0)

#define LOG_ERROR(category, formatStr, ...)                                              \
    do {                                                                                 \
        ::core::log::detail::logf_impl(                                                  \
            ::core::log::Level::Error,                                                   \
            (category),                                                                  \
            __FILE__,                                                                    \
            __LINE__,                                                                    \
            (formatStr) __VA_OPT__(,) __VA_ARGS__);                                      \
    } while (0)

#define LOG_CRITICAL(category, formatStr, ...)                                           \
    do {                                                                                 \
        ::core::log::detail::logf_impl(                                                  \
            ::core::log::Level::Critical,                                                \
            (category),                                                                  \
            __FILE__,                                                                    \
            __LINE__,                                                                    \
            (formatStr) __VA_OPT__(,) __VA_ARGS__);                                      \
    } while (0)

// ------------------------------------------------------------------------------------------------
// Макрос для фатальных (panic) ошибок.
// ------------------------------------------------------------------------------------------------
//
// Поведение реализаций panic/panicf_impl:
//  - логирование на уровне Critical;
//  - принудительный flush лог-файла;
//  - понятный диалог/сообщение пользователю (на русском);
//  - завершение работы процесса.
//
// Использование:
//   LOG_PANIC(core::log::cat::Engine,
//             "Unhandled exception: {}", e.what());
// ------------------------------------------------------------------------------------------------

#define LOG_PANIC(category, formatStr, ...)                                              \
    do {                                                                                 \
        ::core::log::detail::panicf_impl(                                                \
            (category),                                                                  \
            __FILE__,                                                                    \
            __LINE__,                                                                    \
            (formatStr) __VA_OPT__(,) __VA_ARGS__);                                      \
    } while (0)
