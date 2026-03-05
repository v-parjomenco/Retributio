#include "pch.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string_view>

#include "core/config/loader/engine_settings_loader.h"
#include "core/debug/scoped_timer.h"
#include "core/log/log_macros.h"
#include "core/runtime/entry/run_main.h"
#include "config/config_paths.h"
#include "config/loader/spatial_v2_config_builder.h"
#include "config/loader/view_config_loader.h"
#include "config/loader/window_config_loader.h"
#include "dev/spatial_index_harness.h"
#include "presentation/view_manager.h"

namespace {

    void printHarnessException(std::string_view text) noexcept {
        std::fprintf(stderr, "[Error] Spatial harness failed: %.*s\n",
                     static_cast<int>(text.size()), text.data());
        std::fflush(stderr);
    }

    void printHarnessUnknown(std::string_view) noexcept {
        std::fprintf(stderr, "[Error] Spatial harness failed: unknown error.\n");
        std::fflush(stderr);
    }

} // namespace

int main() {

    using namespace std::string_view_literals;

    static constexpr std::array sentinels{
        "assets/config/engine_settings.json"sv,
    };

    core::runtime::entry::EntryConfig cfg{};
    cfg.sentinels = sentinels;
    cfg.workingDirIsFatal = false;
    cfg.singleInstanceEnabled = false;
    cfg.logBuildInfo = true;
    cfg.holdOnExit = false;
    cfg.exceptionPrinter = printHarnessException;
    cfg.unknownExceptionPrinter = printHarnessUnknown;

    return core::runtime::entry::runMain(cfg, []() -> int {
        core::debug::ScopedTimer timer{"Spatial harness session"};

        namespace cfgns = ::core::config;
        namespace skycfg   = ::game::atrapacielos::config;
        namespace skypaths = ::game::atrapacielos::config::paths;

        const skycfg::WindowConfig windowCfg =
            skycfg::loadWindowConfig(skypaths::ATRAPACIELOS_GAME);
        const skycfg::ViewConfig viewCfg =
            skycfg::loadViewConfig(skypaths::ATRAPACIELOS_GAME);
        const cfgns::EngineSettings engineSettings =
            cfgns::loadEngineSettings(skypaths::ENGINE_SETTINGS);

        const sf::Vector2u windowPixelSize{windowCfg.width, windowCfg.height};
        if (windowPixelSize.x == 0u || windowPixelSize.y == 0u) {
            throw std::runtime_error(
                "Spatial harness: shipped window size must be non-zero.");
        }

        game::atrapacielos::presentation::ViewManager viewManager;
        viewManager.init(viewCfg, windowPixelSize);
        const sf::Vector2f worldLogicalSize = viewManager.getWorldLogicalSize();

        const core::ecs::SpatialIndexSystemConfig spatialCfg =
            skycfg::buildSpatialIndexV2ConfigAtrapacielos(engineSettings,
                                                      worldLogicalSize,
                                                      windowPixelSize);

#if !defined(NDEBUG) || defined(RETRIBUTIO_PROFILE)
        game::atrapacielos::dev::tryRunSpatialIndexHarnessFromEnv(spatialCfg);
#else
        LOG_INFO(core::log::cat::Engine,
                 "Spatial harness: недоступен в Release-сборке.");
#endif

        return EXIT_SUCCESS;
    });
}