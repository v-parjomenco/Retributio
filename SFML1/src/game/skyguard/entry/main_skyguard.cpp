#include "pch.h"

#include <array>
#include <cstdlib>
#include <string_view>

#include "core/debug/scoped_timer.h"
#include "core/log/log_macros.h"
#include "core/runtime/entry/run_main.h"
#include "game/skyguard/game.h"

int main() {

    using namespace std::string_view_literals;

    static constexpr std::array sentinels{
        "assets/core/config/engine_settings.json"sv,
        "assets/game/skyguard/config/skyguard_game.json"sv,
    };

    core::runtime::entry::EntryConfig cfg{};
    cfg.sentinels = sentinels;
    cfg.workingDirIsFatal = false;
    cfg.singleInstanceEnabled = true;
    cfg.singleInstanceIsFatal = true;
    cfg.logBuildInfo = true;
    cfg.showErrorDialogOnException = true;
    cfg.showErrorDialogOnSingleGuard = true;
    cfg.holdOnExit = true;
    cfg.exceptionDialogText = "Произошла непредвиденная ошибка.\n"
                              "Подробности — в файле лога в папке 'logs'.";
    cfg.unknownExceptionDialogText = "Произошла неизвестная ошибка.\n"
                                     "Подробности — в файле лога в папке 'logs'.";
    cfg.alreadyRunningDialogText = "SkyGuard уже запущен.";
    cfg.singleGuardOsErrorDialogText = "Не удалось запустить SkyGuard.\n"
                                       "Подробности — в файле лога в папке 'logs'.";

    return core::runtime::entry::runMain(cfg, []() {
        LOG_INFO(core::log::cat::Engine, "Запуск SkyGuard...");

        core::debug::ScopedTimer timer{"Game session"};
        game::skyguard::Game game;
        game.run();

        LOG_INFO(core::log::cat::Engine, "SkyGuard завершён штатно.");
    });
}