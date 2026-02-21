#include "pch.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <new>
#include <string>

#include "core/config/loader/engine_settings_loader.h"
#include "core/log/log_defaults.h"
#include "core/log/log_macros.h"
#include "core/log/logging.h"
#include "game/skyguard/config/config_paths.h"
#include "game/skyguard/config/loader/spatial_v2_config_builder.h"
#include "game/skyguard/config/loader/view_config_loader.h"
#include "game/skyguard/config/loader/window_config_loader.h"
#include "game/skyguard/dev/spatial_index_harness.h"
#include "game/skyguard/presentation/view_manager.h"

namespace {

#ifndef GIT_COMMIT_HASH
    #define GIT_COMMIT_HASH "<unknown>"
#endif

    struct BuildInfo {
        const char* version = "0.1.0-dev";
        const char* buildDate = __DATE__ " " __TIME__;
#if defined(SFML1_PROFILE)
        const char* buildType = "Profile";
#elif defined(NDEBUG)
        const char* buildType = "Release";
#else
        const char* buildType = "Debug";
#endif
        const char* gitCommit = GIT_COMMIT_HASH;
    };

#if defined(_DEBUG) && defined(NDEBUG)
    #error "Invalid build: both _DEBUG and NDEBUG are defined."
#endif
#if defined(SFML1_PROFILE) && defined(_DEBUG)
    #error "Invalid build: Profile must not define _DEBUG (use NDEBUG + SFML1_PROFILE)."
#endif

    void logBuildInfo() {
        const BuildInfo info{};
        LOG_INFO(core::log::cat::Engine,
                 "Build info: version={}, type={}, built at {}, commit={}",
                 info.version,
                 info.buildType,
                 info.buildDate,
                 info.gitCommit);
    }

    class ScopedTimer {
      public:
        explicit ScopedTimer(std::string name) noexcept
            : mName(std::move(name))
            , mStart(std::chrono::steady_clock::now()) {
        }

        ~ScopedTimer() {
            using namespace std::chrono;
            const auto end = steady_clock::now();
            const auto elapsed = duration_cast<milliseconds>(end - mStart).count();

            LOG_DEBUG(core::log::cat::Performance,
                      "[TIMER] {} took {} ms",
                      mName,
                      elapsed);
            (void) elapsed;
        }

      private:
        std::string mName;
        std::chrono::steady_clock::time_point mStart;
    };

    void fixWorkingDirectory(int argc, char* argv[]) {
        namespace fs = std::filesystem;

        fs::path exePath;

        try {
            if (argc > 0 && argv != nullptr && argv[0] != nullptr) {
                exePath = fs::path(argv[0]);

                if (!exePath.is_absolute()) {
                    exePath = fs::absolute(exePath);
                }
            }
        } catch (...) {
            return;
        }

        if (exePath.empty()) {
            return;
        }

        fs::path probe = exePath.parent_path();
        fs::path root = probe.root_path();

        std::error_code ec;

        while (!probe.empty()) {
            const fs::path assetsDir = probe / "assets";
            if (fs::exists(assetsDir, ec) && fs::is_directory(assetsDir, ec)) {
                fs::current_path(probe, ec);
                return;
            }

            if (probe == root) {
                break;
            }

            probe = probe.parent_path();
        }
    }

    void initializeLogging() {
        core::log::Config cfg = core::log::defaults::makeDefaultConfig();
        core::log::init(cfg);

        LOG_INFO(core::log::cat::Engine,
                 "Logging initialized (minLevel={}, console={}, file={}, dir='{}', maxFiles={})",
                 static_cast<int>(cfg.minLevel),
                 cfg.consoleEnabled,
                 cfg.fileEnabled,
                 cfg.logDirectory,
                 cfg.maxFiles);
    }

    void installTerminateHandler() {
        using core::log::cat::Engine;

        std::set_terminate([] {
            static std::atomic<bool> terminating{false};

            if (terminating.exchange(true)) {
                std::abort();
            }

            try {
                if (auto ex = std::current_exception()) {
                    try {
                        std::rethrow_exception(ex);
                    } catch (const std::exception& e) {
                        LOG_PANIC(Engine,
                                  "Unhandled exception (std::terminate): {}",
                                  e.what());
                    } catch (...) {
                        LOG_PANIC(Engine,
                                  "Unhandled non-std exception (std::terminate)");
                    }
                } else {
                    LOG_PANIC(Engine,
                              "std::terminate called without active exception");
                }
            } catch (...) {
                std::abort();
            }
        });
    }

} // namespace

int main(int argc, char* argv[]) {
    installTerminateHandler();
    fixWorkingDirectory(argc, argv);
    initializeLogging();
    logBuildInfo();

    try {
        LOG_INFO(core::log::cat::Engine,
                 "Working directory: {}",
                 std::filesystem::current_path().string());
    } catch (...) {
    }

    try {
        ScopedTimer timer("Spatial harness session");

        namespace cfg = ::core::config;
        namespace skycfg = ::game::skyguard::config;
        namespace skycfg_paths = ::game::skyguard::config::paths;

        const skycfg::WindowConfig windowCfg = skycfg::loadWindowConfig(skycfg_paths::SKYGUARD_GAME);
        const skycfg::ViewConfig viewCfg = skycfg::loadViewConfig(skycfg_paths::SKYGUARD_GAME);
        const cfg::EngineSettings engineSettings = cfg::loadEngineSettings(skycfg_paths::ENGINE_SETTINGS);

        // buildSpatialIndexV2ConfigSkyGuard использует windowPixelSize только для диагностического
        // логирования, не для вычислений spatialCfg. Передаём shipped-конфиг (не user override)
        // для стабильного и воспроизводимого лога между запусками.
        const sf::Vector2u windowPixelSize{windowCfg.width, windowCfg.height};
        if (windowPixelSize.x == 0u || windowPixelSize.y == 0u) {
            throw std::runtime_error("Spatial harness: shipped window size must be non-zero.");
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
                 "Spatial harness: not available in Release build.");
#endif

        try {
            core::log::shutdown();
        } catch (...) {
        }
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        LOG_ERROR(core::log::cat::Engine,
                  "Unhandled exception in spatial harness main(): {}",
                  e.what());
        std::cerr << "[Error] Spatial harness failed: " << e.what() << std::endl;
        try {
            core::log::shutdown();
        } catch (...) {
        }
        return EXIT_FAILURE;
    } catch (...) {
        LOG_ERROR(core::log::cat::Engine,
                  "Unhandled non-std exception in spatial harness main().");
        std::cerr << "[Error] Spatial harness failed: unknown error." << std::endl;
        try {
            core::log::shutdown();
        } catch (...) {
        }
        return EXIT_FAILURE;
    }
}