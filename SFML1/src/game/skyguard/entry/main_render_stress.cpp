#include "pch.h"

#include <array>
#include <cstdlib>
#include <string_view>

#include "core/log/log_macros.h"
#include "core/runtime/entry/run_main.h"

int main() {

    using namespace std::string_view_literals;

    static constexpr std::array sentinels{
        "assets/core/config/engine_settings.json"sv,
    };

    core::runtime::entry::EntryConfig cfg{};
    cfg.sentinels = sentinels;
    cfg.workingDirIsFatal = false;
    cfg.singleInstanceEnabled = false;
    cfg.logBuildInfo = true;
    cfg.holdOnExit = false;

    return core::runtime::entry::runMain(cfg, []() {
        LOG_INFO(core::log::cat::Engine, "Render stress: заглушка — workload отсутствует.");
    });
}