// ================================================================================================
// File: core/log/logging.h
// Purpose: Public API for the engine-wide logging subsystem (core::log).
// Notes:
//  - This header defines the configuration struct and high-level logging functions.
//  - Implementation details (sinks, mutexes, file rotation) live in logging.cpp.
//  - Formatting is provided via logf/logf_impl and uses std::vformat (C++20).
// ================================================================================================
#pragma once

#include <cstddef>
#include <format>
#include <string>
#include <string_view>
#include <utility>

#include "core/log/log_level.h"

namespace core::log {

    /**
     * @brief Конфигурация логгера, задаваемая при инициализации.
     *
     * Здесь нет тяжёлой логики — только набор флагов и параметров по умолчанию.
     * Значения для Debug/Release-сборок задаются в core/log/log_defaults.h.
     */
    struct Config {
        /// @brief Минимальный уровень, который будет записываться в лог.	
        Level           minLevel       = Level::Info;
        /// @brief Включён ли вывод в консоль (std::cout / debugger).		
        bool            consoleEnabled = true;
        /// @brief Включён ли вывод в файл (logs/*.log).		
        bool            fileEnabled    = true;
        /// @brief Должен ли лог-файл сбрасывать буфер при Error+ (Error/Critical).		
        bool            flushOnError   = true;
        /// @brief Каталог, куда складываются лог-файлы (например, "logs").		
        std::string logDirectory = "logs";
        /// @brief Максимальное количество лог-файлов, которые нужно хранить.
        ///        При превышении старые файлы будут удаляться.		
        std::size_t     maxFiles       = 10;
    };

    inline bool operator==(const Config& lhs, const Config& rhs) noexcept {
        return lhs.minLevel       == rhs.minLevel
            && lhs.consoleEnabled == rhs.consoleEnabled
            && lhs.fileEnabled    == rhs.fileEnabled
            && lhs.flushOnError   == rhs.flushOnError
            && lhs.logDirectory   == rhs.logDirectory
            && lhs.maxFiles       == rhs.maxFiles;
    }

    inline bool operator!=(const Config& lhs, const Config& rhs) noexcept {
        return !(lhs == rhs);
    }

    // --------------------------------------------------------------------------------------------
    // Инициализация и завершение работы логгера
    // --------------------------------------------------------------------------------------------
	
    /**
     * @brief Инициализирует подсистему логирования.
     *
     * Открывает лог-файл, настраивает консоль, применяет конфигурацию.
     * Повторный вызов init(config) безопасен: при необходимости будет игнорироваться.
     *
     * Важно:
     *  - Желательно вызывать это одним из первых шагов в main (до загрузки ресурсов).
     *  - Но реализация также может поддерживать "ленивую" инициализацию при первом лог-сообщении.
     */	
    void init(const Config& config);
	
    /**
     * @brief Завершает работу логгера.
     *
     * Закрывает файл, сбрасывает буферы. После shutdown() вызывать log() больше нельзя.
     */	
    void shutdown();

    /**
     * @brief Изменить минимальный уровень логирования в рантайме.
     *
     * Полезно для debug-меню: можно включать/выключать Trace/Debug без перезапуска игры.
     * Потокобезопасна: внутри берёт mutex логгера и безопасно обновляет Config.
     */
    void setMinLevel(Level level);

    // --------------------------------------------------------------------------------------------
    // Базовая функция логирования (без форматирования)
    // --------------------------------------------------------------------------------------------

    /**
     * @brief Низкоуровневый вызов логгера с уже подготовленной строкой.
     *
     * Обычно напрямую не используется игровым кодом — вместо этого удобнее применять макросы
     * LOG_INFO/LOG_WARN/... и форматируемые обёртки logf/logf_impl.
     *
     * @param level    Уровень сообщения (Info, Warning, ...).
     * @param category Категория (см. core/log/log_categories.h).
     * @param message  Готовый текст сообщения (UTF-8).
     * @param file     Имя исходного файла (обычно __FILE__).
     * @param line     Номер строки (обычно __LINE__).
     */
    void log(Level level,
             std::string_view category,
             std::string_view message,
             const char* file,
             int line);

    // --------------------------------------------------------------------------------------------
    // Форматируемые версии (логика реализована через std::vformat)
    // --------------------------------------------------------------------------------------------

    /**
     * @brief Форматируемый вариант log с std::format-style синтаксисом.
     *
     * Пример использования:
     *  - core::log::logf(Level::Info, core::log::cat::Resources,
     *                    "Loaded {} textures", count);
     *
     * Как правило, удобнее использовать макросы LOG_INFO/LOG_ERROR, которые под капотом
     * вызывают detail::logf_impl(...) с __FILE__/__LINE__.
     */
    template <typename... Args>
    void logf(Level level,
              std::string_view category,
              const char* formatString,
              Args&&... args);

    /**
     * @brief Фатальное сообщение (panic) с уже готовой строкой.
     *
     * Поведение:
     *  - записывает сообщение на уровне Critical;
     *  - принудительно сбрасывает лог-файл;
     *  - показывает пользователю диалог (MessageBox / stderr) с понятным текстом;
     *  - завершает работу процесса (std::exit).
     *
     * Эта функция предназначена для фатальных, невосстановимых ситуаций.
     */
    [[noreturn]] void panic(std::string_view category,
                            std::string_view message,
                            const char* file,
                            int line);

    /**
     * @brief Форматируемая версия panic (panic + std::format).
     *
     * Пример:
     *  - core::log::panicf(core::log::cat::Engine,
     *      "Unhandled exception: {}", e.what());
     *
     * Обычно вызывается через макрос LOG_PANIC(...).
     */
    template <typename... Args>
    void panicf(std::string_view category,
                const char* formatString,
                Args&&... args);

    // --------------------------------------------------------------------------------------------
    // Внутренние helper'ы для макросов (log_macros.h)
    // --------------------------------------------------------------------------------------------

    namespace detail {

        /**
         * @brief Внутренний helper: форматируемое логирование с привязкой к файлу/строке.
         *
         * Используется макросами LOG_TRACE/LOG_DEBUG/LOG_INFO/...:
         *  - добавляет __FILE__/__LINE__;
         *  - форматирует строку через std::vformat;
         *  - вызывает core::log::log(...).
         */
        template <typename... Args>
        void logf_impl(Level level,
                       std::string_view category,
                       const char* file,
                       int line,
                       const char* formatString,
                       Args&&... args);

        /**
         * @brief Внутренний helper для фатальных ошибок (panic) с форматированием.
         *
         * Используется макросом LOG_PANIC(...).
         */
        template <typename... Args>
        void panicf_impl(std::string_view category,
                         const char* file,
                         int line,
                         const char* formatString,
                         Args&&... args);
    }

    // ============================================================================================
    // Шаблонные реализации
    // ============================================================================================

    template <typename... Args>
    inline void logf(Level level,
                     std::string_view category,
                     const char* formatString,
                     Args&&... args) {
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
        inline void logf_impl(Level level, std::string_view category, const char* file, int line,
                              const char* formatString, Args&&... args) {
            try {
                // NB: make_format_args принимает const Args&..., поэтому forwarding не нужен.
                auto fmtArgs = std::make_format_args(args...);

                std::string formatted =
                    std::vformat(std::string_view{formatString ? formatString : ""}, fmtArgs);

                log(level, category, formatted, file, line);
            } catch (const std::format_error& e) {
                // Фоллбэк: если форматирование сломалось, логируем саму проблему.
                std::string fallback = "[Log formatting error] ";
                fallback += e.what();
                fallback += " | formatStr: ";
                fallback += formatString ? formatString : "<null>";
                log(Level::Error, category, fallback, file, line);
            }
        }

        template <typename... Args>
        inline void panicf_impl(std::string_view category, const char* file, int line,
                                const char* formatString, Args&&... args) {
            try {
                auto fmtArgs = std::make_format_args(args...);

                std::string formatted =
                    std::vformat(std::string_view{formatString ? formatString : ""}, fmtArgs);

                panic(category, formatted, file, line);
            } catch (const std::format_error& e) {
                // Если не удалось даже отформатировать panic-сообщение — логируем упрощённый вариант.
                std::string fallback = "[PANIC formatting error] ";
                fallback += e.what();
                fallback += " | formatStr: ";
                fallback += formatString ? formatString : "<null>";
                panic(category, fallback, file, line);
            }
        }

    } // namespace detail

} // namespace core::log