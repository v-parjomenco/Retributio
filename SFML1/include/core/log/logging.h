// ================================================================================================
// File: core/log/logging.h
// Purpose: Public API for the engine-wide logging subsystem (core::log).
//
// Architecture (ADR):
//  - Synchronous logger, thread-safe via mutex. No async queue.
//  - On enabled log:  1 string build (envelope + payload in one buffer), no intermediate
//                      payload allocation. Scratch buffer reused across calls.
//  - On disabled log: 0 formatting, 0 lock (atomic fast-gate in macro/wouldLog).
//  - Format strings:  compile-time checked via std::format_string<Args...>.
//                      Runtime format strings are NOT supported through the macro/template path.
//                      Mods/scripts log via log() with a pre-formatted std::string_view message;
//                      formatting is the caller's responsibility.
//  - Layout:          Debug  — [timestamp] [LEVEL] [category] message (file:line)
//                     Release — [timestamp] [LEVEL] [category] message
//  - Flush policy:    console and file flush only on Error+ and PANIC.
//  - Ordering:        global order guaranteed (single mutex).
//  - Crash semantics: PANIC guarantees flush + close before process exit.
// ================================================================================================
#pragma once

#include <atomic>
#include <cstddef>
#include <format>
#include <string>
#include <string_view>

#include "core/log/log_level.h"

namespace core::log {

    // --------------------------------------------------------------------------------------------
    // Конфигурация
    // --------------------------------------------------------------------------------------------

    /**
     * @brief Конфигурация логгера, задаваемая при инициализации.
     *
     * Значения для Debug/Release-сборок задаются в core/log/log_defaults.h.
     */
    struct Config {
        /// @brief Минимальный уровень, который будет записываться в лог.
        Level       minLevel       = Level::Info;
        /// @brief Включён ли вывод в консоль (std::cout / debugger).
        bool        consoleEnabled = true;
        /// @brief Включён ли вывод в файл (logs/*.log).
        bool        fileEnabled    = true;
        /// @brief Должен ли лог-файл сбрасывать буфер при Error+ (Error/Critical).
        bool        flushOnError   = true;
        /// @brief Каталог, куда складываются лог-файлы (например, "logs").
        std::string logDirectory   = "logs";
        /// @brief Максимальное количество лог-файлов, которые нужно хранить.
        std::size_t maxFiles       = 10;
    };

    inline bool operator==(const Config& lhs, const Config& rhs) noexcept {
        return lhs.minLevel       == rhs.minLevel
            && lhs.consoleEnabled == rhs.consoleEnabled
            && lhs.fileEnabled    == rhs.fileEnabled
            && lhs.flushOnError   == rhs.flushOnError
            && lhs.logDirectory   == rhs.logDirectory
            && lhs.maxFiles       == rhs.maxFiles;
    }

    // --------------------------------------------------------------------------------------------
    // Fast-path гейт (уровень макросов): не форматируем, если сообщение будет отброшено.
    // --------------------------------------------------------------------------------------------

    // g_fastGateReady == false => «консервативный режим»: возвращаем true,
    //     чтобы не потерять ранние логи до init() / ленивой инициализации.
    // После инициализации: g_fastGateReady == true, гейтимся по g_minLevelAtomic без mutex.

    inline std::atomic<Level> g_minLevelAtomic{Level::Info};
    inline std::atomic<bool>  g_fastGateReady{false};

    /**
     * @brief Быстрая проверка «будет ли лог записан» без захвата mutex.
     *
     * Используется макросами перед любым форматированием.
     * category оставлен на будущее (per-category filtering); сейчас не учитывается.
     */
    [[nodiscard]] inline bool wouldLog(Level level, std::string_view /*category*/) noexcept {
        if (!g_fastGateReady.load(std::memory_order_acquire)) {
            return true;
        }
        return level >= g_minLevelAtomic.load(std::memory_order_relaxed);
    }

    // --------------------------------------------------------------------------------------------
    // Жизненный цикл
    // --------------------------------------------------------------------------------------------

    void init(const Config& config);
    void shutdown();
    void setMinLevel(Level level);

    // --------------------------------------------------------------------------------------------
    // Путь с готовым сообщением (для модов/скриптов/прямого использования).
    // Форматирование — ответственность вызывающей стороны.
    // --------------------------------------------------------------------------------------------

    void log(Level level,
             std::string_view category,
             std::string_view message,
             const char* file,
             int line);

    [[noreturn]] void panic(std::string_view category,
                            std::string_view message,
                            const char* file,
                            int line);

    // --------------------------------------------------------------------------------------------
    // Форматируемый путь — non-template sink'и (реализация в logging.cpp).
    // --------------------------------------------------------------------------------------------

    namespace detail {

        void log_write(Level level,
                       std::string_view category,
                       const char* file,
                       int line,
                       std::string_view fmt,
                       std::format_args args);

        [[noreturn]] void panic_write(std::string_view category,
                                      const char* file,
                                      int line,
                                      std::string_view fmt,
                                      std::format_args args);

    } // namespace detail

    // --------------------------------------------------------------------------------------------
    // Форматируемый путь — тонкие шаблонные точки входа.
    //
    // Делают ровно две вещи:
    //  1. Compile-time валидация формата (std::format_string<Args...>).
    //  2. Type-erasure (std::make_format_args) + немедленный вызов non-template sink в .cpp.
    //
    // Никакого std::vformat, промежуточных строк или тяжёлой STL-работы в header.
    // --------------------------------------------------------------------------------------------

    namespace detail {

        /// Вызывается макросами LOG_* (с __FILE__/__LINE__).
        template <typename... Args>
        void logf_impl(Level level,
                       std::string_view category,
                       const char* file,
                       int line,
                       std::format_string<Args...> fmt,
                       Args&&... args) {
            log_write(level, category, file, line,
                      fmt.get(), std::make_format_args(args...));
        }

        /// Вызывается макросом LOG_PANIC (с __FILE__/__LINE__). [[noreturn]].
        template <typename... Args>
        [[noreturn]] void panicf_impl(std::string_view category,
                                      const char* file,
                                      int line,
                                      std::format_string<Args...> fmt,
                                      Args&&... args) {
            panic_write(category, file, line,
                        fmt.get(), std::make_format_args(args...));
        }

    } // namespace detail

    // --------------------------------------------------------------------------------------------
    // Удобные функции (без file/line, для прямого использования вне макросов).
    // --------------------------------------------------------------------------------------------

    /// Форматированный лог — compile-time проверка. Source location помечен как неизвестный.
    template <typename... Args>
    void logf(Level level,
              std::string_view category,
              std::format_string<Args...> fmt,
              Args&&... args) {
        if (!wouldLog(level, category)) {
            return;
        }
        detail::log_write(level, category, "", 0,
                          fmt.get(), std::make_format_args(args...));
    }

    /// Форматированный panic — compile-time проверка. Source location помечен как неизвестный.
    template <typename... Args>
    [[noreturn]] void panicf(std::string_view category,
                             std::format_string<Args...> fmt,
                             Args&&... args) {
        detail::panic_write(category, "", 0,
                            fmt.get(), std::make_format_args(args...));
    }

} // namespace core::log