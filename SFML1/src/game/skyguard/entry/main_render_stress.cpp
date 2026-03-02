#include "pch.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>

#include "core/log/log_defaults.h"
#include "core/log/log_macros.h"
#include "core/log/logging.h"
#include "core/runtime/entry/build_info.h"
#include "core/runtime/entry/logging_lifetime.h"
#include "core/runtime/entry/terminate_handler.h"
#include "core/runtime/entry/working_directory.h"

int main() {
    // Bootstrap идентичен spatial_harness: dev-инструмент без GUI и single-instance guard.
    // SingleInstanceGuard, showError и holdOnExitIfEnabled добавить
    // при появлении реального workload.
    core::runtime::entry::installTerminateHandler();

    using namespace std::string_view_literals;
    static constexpr std::array sentinels{
        "assets/core/config/engine_settings.json"sv,
    };
    if (!core::runtime::entry::setWorkingDirectoryFromExecutable(sentinels)) {
        // Логгер ещё не инициализирован — stderr единственный канал.
        // Не fatal: стресс-тест продолжит работу, но assets могут не найтись.
        std::cerr << "[Warning] Failed to locate project root from executable path.\n";
    }

    core::runtime::entry::LoggingLifetime logging{core::log::defaults::makeDefaultConfig()};
    core::runtime::entry::logBuildInfo();

    LOG_INFO(core::log::cat::Engine, "Render stress: заглушка — workload отсутствует.");
    return EXIT_SUCCESS;
    // ~LoggingLifetime() → core::log::shutdown() — гарантировано RAII.
}