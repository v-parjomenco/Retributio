// ================================================================================================
// File: core/log/log_defaults.h
// Purpose: Build-configuration-specific defaults for the logging subsystem.
// Notes:
//  - Provides default values for Debug/Release builds.
//  - Used to bootstrap core::log::Config in main/bootstrap code.
// ================================================================================================
#pragma once

#include <cstddef>
#include <string_view>

#include "core/log/log_level.h"
#include "core/log/logging.h"

namespace core::log::defaults {

    /**
     * @brief Значения по умолчанию для различных конфигураций сборки.
     *
     * Здесь мы жёстко шьём разумные дефолты:
     *  - В Debug:
     *      * много деталей (Trace/Debug),
     *      * включён вывод в консоль,
     *      * включён вывод в лог-файл.
     *  - В Release:
     *      * только существенная информация (Info и выше),
     *      * вывод в консоль по умолчанию отключён (можно включить вручную при необходимости),
     *      * лог-файл остаётся включённым.
     */

#ifdef _DEBUG
    inline constexpr Level MinLevel       = Level::Trace;
    inline constexpr bool  ConsoleEnabled = true;
    inline constexpr bool  FileEnabled    = true;
#else
    inline constexpr Level MinLevel       = Level::Info;
    inline constexpr bool  ConsoleEnabled = false;
    inline constexpr bool  FileEnabled    = true;
#endif

    inline constexpr bool               FlushOnError  = true;
    inline constexpr std::string_view   LogDirectory  = "logs";
    inline constexpr std::size_t        MaxLogFiles   = 10;

    /**
     * @brief Создаёт конфигурацию логгера с дефолтами для текущей сборки.
     * 
     * Без побочных эффектов, без логирования, только заполнение struct Config.
     *
     * Использование:
     *  - core::log::Config cfg = core::log::defaults::makeDefaultConfig();
     *  - core::log::init(cfg);
     */
    inline Config makeDefaultConfig() noexcept {
        Config cfg{};
        cfg.minLevel = MinLevel;
        cfg.consoleEnabled = ConsoleEnabled;
        cfg.fileEnabled = FileEnabled;
        cfg.flushOnError = FlushOnError;
        cfg.logDirectory = LogDirectory;
        cfg.maxFiles = MaxLogFiles;
        return cfg;
    }

} // namespace core::log::defaults