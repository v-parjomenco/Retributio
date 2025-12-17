// ================================================================================================
// File: core/log/logging.h
// Purpose: Public API for the engine-wide logging subsystem (core::log).
// Notes:
//  - This header defines the configuration struct and high-level logging functions.
//  - Implementation details (sinks, mutexes, file rotation) live in logging.cpp.
//  - Formatting is provided via logf/detail::logf_impl and uses std::vformat (C++20).
// ================================================================================================
#pragma once

#include <atomic>
#include <cstddef>
#include <format>
#include <string>
#include <string_view>
#include <utility>

#include "core/log/log_level.h"

// -----------------------------------------------------------------------------------------------
// Хелперы форматирования логов намеренно НЕ заинлайнены.
// Они тяжелые (std::vformat + выделение памяти) и могут вызывать /Wall C4710 под /WX в Release.
// ------------------------------------------------------------------------------------------------
#if defined(_MSC_VER)
    #define CORE_LOG_NOINLINE __declspec(noinline)
#elif defined(__clang__) || defined(__GNUC__)
    #define CORE_LOG_NOINLINE __attribute__((noinline))
#else
    #define CORE_LOG_NOINLINE
#endif

namespace core::log {

    /**
     * @brief Конфигурация логгера, задаваемая при инициализации.
     *
     * Здесь нет тяжёлой логики — только набор флагов и параметров по умолчанию.
     * Значения для Debug/Release-сборок задаются в core/log/log_defaults.h.
     */
    struct Config {
        /// @brief Минимальный уровень, который будет записываться в лог.
        Level       minLevel        = Level::Info;
        /// @brief Включён ли вывод в консоль (std::cout / debugger).
        bool        consoleEnabled  = true;
        /// @brief Включён ли вывод в файл (logs/*.log).
        bool        fileEnabled     = true;
        /// @brief Должен ли лог-файл сбрасывать буфер при Error+ (Error/Critical).
        bool        flushOnError    = true;
        /// @brief Каталог, куда складываются лог-файлы (например, "logs").
        std::string logDirectory    = "logs";
        /// @brief Максимальное количество лог-файлов, которые нужно хранить.
        ///        При превышении старые файлы будут удаляться.
        std::size_t maxFiles        = 10;
    };

    inline bool operator==(const Config& lhs, const Config& rhs) noexcept {
        return lhs.minLevel == rhs.minLevel
            && lhs.consoleEnabled == rhs.consoleEnabled
            && lhs.fileEnabled == rhs.fileEnabled
            && lhs.flushOnError == rhs.flushOnError
            && lhs.logDirectory == rhs.logDirectory
            && lhs.maxFiles == rhs.maxFiles;
    }

    inline bool operator!=(const Config& lhs, const Config& rhs) noexcept {
        return !(lhs == rhs);
    }

    // --------------------------------------------------------------------------------------------
    // Fast-path gating (macro-level): avoid std::vformat when message will be dropped anyway.
    // --------------------------------------------------------------------------------------------

    // NB:
    //  - g_fastGateReady false => "консервативный режим": 
    //    возвращаем true, чтобы не потерять ранние логи до init()/ленивой инициализации.
    //  - После инициализации (initLocked/ensureInitialized/setMinLevel)
    //    g_fastGateReady становится true, и гейтимся по minLevel атомиком без mutex.

    inline std::atomic<Level> g_minLevelAtomic{Level::Info};
    inline std::atomic<bool>  g_fastGateReady{false};

    /**
     * @brief Быстрая проверка "будет ли лог вообще записан" по текущему minLevel.
     *
     * Используется макросами, чтобы не делать форматирование (std::vformat) и не вычислять
     * аргументы зря.
     * Сейчас category не учитывается (у нас глобальный minLevel), но параметр оставлен на будущее.
     */
    [[nodiscard]] inline bool wouldLog(Level level, std::string_view /*category*/) noexcept {
        if (!g_fastGateReady.load(std::memory_order_acquire)) {
            return true;
        }

        const Level minLevel = g_minLevelAtomic.load(std::memory_order_relaxed);
        return level >= minLevel;
    }

    // --------------------------------------------------------------------------------------------
    // Инициализация и завершение работы логгера
    // --------------------------------------------------------------------------------------------

    void init(const Config& config);
    void shutdown();
    void setMinLevel(Level level);

    // --------------------------------------------------------------------------------------------
    // Базовая функция логирования (без форматирования)
    // --------------------------------------------------------------------------------------------

    void log(Level level,
             std::string_view category,
             std::string_view message,
             const char* file,
             int line);

    // --------------------------------------------------------------------------------------------
    // Форматируемые версии (логика реализована через std::vformat)
    // --------------------------------------------------------------------------------------------

    template <typename... Args>
    void logf(Level level,
              std::string_view category,
              const char* formatString,
              Args&&... args);

    [[noreturn]] void panic(std::string_view category,
                            std::string_view message,
                            const char* file,
                            int line);

    template <typename... Args>
    void panicf(std::string_view category,
                const char* formatString,
                Args&&... args);

    // --------------------------------------------------------------------------------------------
    // Внутренние helper'ы для макросов (log_macros.h)
    // --------------------------------------------------------------------------------------------

    namespace detail {

        #if defined(_MSC_VER)
            #pragma warning(push)
            // /Wall emits C4710/C4711 for heavy STL formatting code in headers,
            // and /WX makes it fatal.
            // This is noise (not a bug) and depends on MSVC inlining heuristics
            // (Win32 triggers it more often).
            #pragma warning(disable : 4710) // function not inlined
            #pragma warning(disable : 4711) // selected for automatic inline expansion
        #endif

        template <typename... Args>
        CORE_LOG_NOINLINE void logf_impl(Level level,
                                         std::string_view category,
                                         const char* file,
                                         int line,
                                         const char* formatString,
                                         Args&&... args);

        template <typename... Args>
        CORE_LOG_NOINLINE void panicf_impl(std::string_view category,
                                           const char* file,
                                           int line,
                                           const char* formatString,
                                           Args&&... args);

    } // namespace detail

    // --------------------------------------------------------------------------------------------
    // Шаблонные реализации
    // --------------------------------------------------------------------------------------------

    template <typename... Args>
    inline void logf(Level level,
                     std::string_view category,
                     const char* formatString,
                     Args&&... args) {
        // Fast-path: не форматируем вообще, если сообщение будет отброшено по minLevel.
        if (!wouldLog(level, category)) {
            return;
        }

        // Вариант без файла/строки: помечаем как неизвестный источник.
        detail::logf_impl(level,
                          category,
                          /*file*/ "",
                          /*line*/ 0,
                          formatString,
                          std::forward<Args>(args)...);
    }

    template <typename... Args>
    inline void panicf(std::string_view category,
                       const char* formatString,
                       Args&&... args) {
        detail::panicf_impl(category,
                            /*file*/ "",
                            /*line*/ 0,
                            formatString,
                            std::forward<Args>(args)...);
    }

    namespace detail {

        template <typename... Args>
        CORE_LOG_NOINLINE void logf_impl(Level level,
                                         std::string_view category,
                                         const char* file,
                                         int line,
                                         const char* formatString,
                                         Args&&... args) {
            try {
                // NB: make_format_args принимает const Args&..., поэтому forwarding не нужен.
                auto fmtArgs = std::make_format_args(args...);

                const std::string_view fmt =
                    formatString ? std::string_view{formatString} : std::string_view{};
                std::string formatted = std::vformat(fmt, fmtArgs);

                log(level, category, formatted, file, line);
            } catch (const std::format_error& e) {
                std::string fallback = "[Log formatting error] ";
                fallback += e.what();
                fallback += " | formatStr: ";
                fallback += formatString ? formatString : "<null>";

                log(Level::Error, category, fallback, file, line);
            }
        }

        template <typename... Args>
        CORE_LOG_NOINLINE void panicf_impl(std::string_view category,
                                           const char* file,
                                           int line,
                                           const char* formatString,
                                           Args&&... args) {
            try {
                auto fmtArgs = std::make_format_args(args...);

                const std::string_view fmt =
                    formatString ? std::string_view{formatString} : std::string_view{};
                std::string formatted = std::vformat(fmt, fmtArgs);

                panic(category, formatted, file, line);
            } catch (const std::format_error& e) {
                std::string fallback = "[PANIC formatting error] ";
                fallback += e.what();
                fallback += " | formatStr: ";
                fallback += formatString ? formatString : "<null>";

                panic(category, fallback, file, line);
            }
        }

        #if defined(_MSC_VER)
            #pragma warning(pop)
        #endif

    } // namespace detail

} // namespace core::log

#undef CORE_LOG_NOINLINE