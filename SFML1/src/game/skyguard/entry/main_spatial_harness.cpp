#include "pch.h"

#include <array>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string_view>

#include "core/config/loader/engine_settings_loader.h"
#include "core/debug/scoped_timer.h"
#include "core/log/log_defaults.h"
#include "core/log/log_macros.h"
#include "core/log/logging.h"
#include "core/runtime/entry/build_info.h"
#include "core/runtime/entry/logging_lifetime.h"
#include "core/runtime/entry/terminate_handler.h"
#include "core/runtime/entry/working_directory.h"
#include "game/skyguard/config/config_paths.h"
#include "game/skyguard/config/loader/spatial_v2_config_builder.h"
#include "game/skyguard/config/loader/view_config_loader.h"
#include "game/skyguard/config/loader/window_config_loader.h"
#include "game/skyguard/dev/spatial_index_harness.h"
#include "game/skyguard/presentation/view_manager.h"

int main() {
    // 1. Terminate handler — первым, до любой инициализации.
    core::runtime::entry::installTerminateHandler();

    // 2. Рабочий каталог — до логгера.
    //    Харнес является dev-инструментом, достаточно одного sentinel (game-sentinel не нужен).
    using namespace std::string_view_literals;
    static constexpr std::array sentinels{
        "assets/core/config/engine_settings.json"sv,
    };
    if (!core::runtime::entry::setWorkingDirectoryFromExecutable(sentinels)) {
        // Логгер ещё не инициализирован — stderr единственный канал.
        // Не fatal: харнес продолжит работу, но assets могут не найтись.
        std::cerr << "[Warning] Failed to locate project root from executable path.\n";
    }

    // 3. Логгер (RAII): shutdown() гарантирован при любом пути выхода.
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

    // 5. Доменная логика харнеса.
    //    SingleInstanceGuard намеренно отсутствует: параллельный запуск dev-инструментов допустим.
    //    showError/holdOnExitIfEnabled намеренно отсутствуют: консольный инструмент,
    //    stderr достаточен.
    try {
        core::debug::ScopedTimer timer{"Spatial harness session"};

        namespace cfg      = ::core::config;
        namespace skycfg   = ::game::skyguard::config;
        namespace skypaths = ::game::skyguard::config::paths;

        const skycfg::WindowConfig windowCfg =
            skycfg::loadWindowConfig(skypaths::SKYGUARD_GAME);
        const skycfg::ViewConfig viewCfg =
            skycfg::loadViewConfig(skypaths::SKYGUARD_GAME);
        const cfg::EngineSettings engineSettings =
            cfg::loadEngineSettings(skypaths::ENGINE_SETTINGS);

        // buildSpatialIndexV2ConfigSkyGuard использует windowPixelSize только для
        // диагностического логирования, не для вычислений spatialCfg. Передаём
        // shipped-конфиг (не user override) для стабильного воспроизводимого лога.
        const sf::Vector2u windowPixelSize{windowCfg.width, windowCfg.height};
        if (windowPixelSize.x == 0u || windowPixelSize.y == 0u) {
            throw std::runtime_error(
                "Spatial harness: shipped window size must be non-zero.");
        }

        game::skyguard::presentation::ViewManager viewManager;
        viewManager.init(viewCfg, windowPixelSize);
        const sf::Vector2f worldLogicalSize = viewManager.getWorldLogicalSize();

        const core::ecs::SpatialIndexSystemConfig spatialCfg =
            skycfg::buildSpatialIndexV2ConfigSkyGuard(engineSettings,
                                                      worldLogicalSize,
                                                      windowPixelSize);

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        game::skyguard::dev::tryRunSpatialIndexHarnessFromEnv(spatialCfg);
#else
        LOG_INFO(core::log::cat::Engine,
                 "Spatial harness: недоступен в Release-сборке.");
#endif

        return EXIT_SUCCESS;

    } catch (const std::exception& e) {
        LOG_ERROR(core::log::cat::Engine,
                  "Необработанное исключение в main() харнеса: {}", e.what());
        std::cerr << "[Error] Spatial harness failed: " << e.what() << '\n';
        return EXIT_FAILURE;
    } catch (...) {
        LOG_ERROR(core::log::cat::Engine,
                  "Необработанное исключение неизвестного типа в main() харнеса.");
        std::cerr << "[Error] Spatial harness failed: unknown error.\n";
        return EXIT_FAILURE;
    }
    // ~LoggingLifetime() → core::log::shutdown() — гарантировано RAII.
}