#include "pch.h"

#include <array>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <string_view>

#include "core/debug/scoped_timer.h"
#include "core/log/log_defaults.h"
#include "core/log/log_macros.h"
#include "core/log/logging.h"
#include "core/runtime/entry/build_info.h"
#include "core/runtime/entry/logging_lifetime.h"
#include "core/runtime/entry/single_instance_guard.h"
#include "core/runtime/entry/terminate_handler.h"
#include "core/runtime/entry/user_dialog.h"
#include "core/runtime/entry/working_directory.h"
#include "game/skyguard/game.h"

int main() {
    // 1. Terminate handler — первым, до любой инициализации.
    //    Даёт хотя бы stderr-диагностику при падении в static-инициализаторах.
    core::runtime::entry::installTerminateHandler();

    // 2. Рабочий каталог — до логгера: логгер открывает logs/ относительно cwd.
    //    Оба sentinel обязательны: отсеивают случайные директории с assets/.
    using namespace std::string_view_literals;
    static constexpr std::array sentinels{
        "assets/core/config/engine_settings.json"sv,
        "assets/game/skyguard/config/skyguard_game.json"sv,
    };
    core::runtime::entry::setWorkingDirectoryFromExecutable(sentinels);

    // 3. Логгер (RAII): shutdown() гарантирован при любом пути выхода из main().
    core::runtime::entry::LoggingLifetime logging{core::log::defaults::makeDefaultConfig()};

    // 4. Трассировочная информация о сборке.
    core::runtime::entry::logBuildInfo();

    try {
        LOG_INFO(core::log::cat::Engine,
                 "Рабочий каталог: {}",
                 std::filesystem::current_path().string());
    } catch (...) {
        // current_path() может бросить если cwd стал недоступен — не критично.
    }

    // 5. Защита от одновременного запуска нескольких экземпляров.
    core::runtime::entry::SingleInstanceGuard instanceGuard;
    using enum core::runtime::entry::SingleInstanceStatus;
    switch (instanceGuard.status()) {
        case AlreadyRunning:
            LOG_WARN(core::log::cat::Engine,
                     "Другой экземпляр уже запущен. Завершение.");
            core::runtime::entry::showError("SkyGuard уже запущен.");
            core::runtime::entry::holdOnExitIfEnabled();
            return EXIT_SUCCESS;

        case OsError:
            LOG_ERROR(core::log::cat::Engine,
                      "Не удалось захватить блокировку единственного экземпляра (OS error).");
            core::runtime::entry::showError(
                "Не удалось запустить SkyGuard.\n"
                "Подробности — в файле лога в папке 'logs'.");
            core::runtime::entry::holdOnExitIfEnabled();
            return EXIT_FAILURE;

        case Acquired:
            break;

        case Inactive:
            // Свежесозданный guard не может быть Inactive — инвариант нарушен.
            std::abort();
    }

    // 6. Игровой цикл.
    try {
        LOG_INFO(core::log::cat::Engine, "Запуск SkyGuard...");

        core::debug::ScopedTimer timer{"Game session"};
        game::skyguard::Game game;
        game.run();

        LOG_INFO(core::log::cat::Engine, "SkyGuard завершён штатно.");
    } catch (const std::exception& e) {
        LOG_ERROR(core::log::cat::Engine,
                  "Необработанное исключение в main(): {}", e.what());
        core::runtime::entry::showError(
            "Произошла непредвиденная ошибка.\n"
            "Подробности — в файле лога в папке 'logs'.");
        core::runtime::entry::holdOnExitIfEnabled();
        return EXIT_FAILURE;
    } catch (...) {
        LOG_ERROR(core::log::cat::Engine,
                  "Необработанное исключение неизвестного типа в main().");
        core::runtime::entry::showError(
            "Произошла неизвестная ошибка.\n"
            "Подробности — в файле лога в папке 'logs'.");
        core::runtime::entry::holdOnExitIfEnabled();
        return EXIT_FAILURE;
    }

    core::runtime::entry::holdOnExitIfEnabled();
    return EXIT_SUCCESS;
    // ~LoggingLifetime() → core::log::shutdown() — гарантировано RAII.
}