// ================================================================================================
// File: core/runtime/entry/run_main.h
// Purpose: Shared entry bootstrap runner for executable mains.
// Notes:
//  - Installs terminate handler first.
//  - Applies working directory policy before logger initialization.
//  - Owns logger lifetime via LoggingLifetime RAII.
//  - Optionally enforces single-instance policy.
// ================================================================================================
#pragma once

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

#include "core/log/log_defaults.h"
#include "core/log/log_macros.h"
#include "core/runtime/entry/build_info.h"
#include "core/runtime/entry/logging_lifetime.h"
#include "core/runtime/entry/single_instance_guard.h"
#include "core/runtime/entry/terminate_handler.h"
#include "core/runtime/entry/user_dialog.h"
#include "core/runtime/entry/working_directory.h"

namespace core::runtime::entry {

    struct EntryConfig {
        std::span<const std::string_view> sentinels{};

        bool workingDirIsFatal            = false;
        bool singleInstanceEnabled        = false;
        bool singleInstanceIsFatal        = true;
        bool logBuildInfo                 = true;
        bool showErrorDialogOnException   = false;
        bool showErrorDialogOnSingleGuard = false;
        bool holdOnExit                   = false;

        std::string_view workingDirWarningText =
            "Failed to locate project root from executable path.";
        std::string_view workingDirFatalText =
            "Failed to locate project root from executable path.";
        std::string_view exceptionDialogText =
            "Unexpected error occurred.\nSee logs in 'logs' directory.";
        std::string_view unknownExceptionDialogText =
            "Unknown error occurred.\nSee logs in 'logs' directory.";
        std::string_view alreadyRunningDialogText = "Application is already running.";
        std::string_view singleGuardOsErrorDialogText =
            "Failed to acquire single-instance lock.\nSee logs in 'logs' directory.";

        void (*preLogWarningPrinter)(std::string_view text) noexcept = nullptr;
        void (*exceptionPrinter)(std::string_view text) noexcept = nullptr;
        void (*unknownExceptionPrinter)(std::string_view text) noexcept = nullptr;
    };

    namespace detail {

        inline void printStderr(std::string_view text) noexcept {
            std::fprintf(stderr,
                         "%.*s\n",
                         static_cast<int>(text.size()),
                         text.data());
            std::fflush(stderr);
        }

        inline void printWarningStderr(std::string_view text) noexcept {
            std::fprintf(stderr,
                         "[Warning] %.*s\n",
                         static_cast<int>(text.size()),
                         text.data());
            std::fflush(stderr);
        }

    } // namespace detail

    template <class Body>
    int runMain(const EntryConfig& cfg, Body&& body) {
        static_assert(std::is_invocable_v<Body&&>,
                      "runMain: body must be callable with no arguments.");

        using BodyResult = std::invoke_result_t<Body&&>;
        static_assert(std::is_void_v<BodyResult> || std::is_same_v<BodyResult, int>,
                      "runMain: body must return void or int.");

        static_assert(!std::is_copy_constructible_v<SingleInstanceGuard>,
                      "SingleInstanceGuard must be non-copyable.");
        static_assert(!std::is_copy_assignable_v<SingleInstanceGuard>,
                      "SingleInstanceGuard must be non-copy-assignable.");
        static_assert(std::is_nothrow_default_constructible_v<SingleInstanceGuard>,
                      "SingleInstanceGuard default ctor must be noexcept.");
        static_assert(std::is_move_constructible_v<SingleInstanceGuard>,
                      "SingleInstanceGuard must be move-constructible.");
        static_assert(std::is_nothrow_move_constructible_v<SingleInstanceGuard>,
                      "SingleInstanceGuard move must be noexcept.");

        installTerminateHandler();

        const bool cwdOk = setWorkingDirectoryFromExecutable(cfg.sentinels);
        if (!cwdOk) {
            if (cfg.workingDirIsFatal) {
                detail::printStderr(cfg.workingDirFatalText);
                if (cfg.holdOnExit) {
                    holdOnExitIfEnabled();
                }
                return EXIT_FAILURE;
            }

            if (cfg.preLogWarningPrinter != nullptr) {
                cfg.preLogWarningPrinter(cfg.workingDirWarningText);
            } else {
                detail::printWarningStderr(cfg.workingDirWarningText);
            }
        }

        LoggingLifetime logging{core::log::defaults::makeDefaultConfig()};

        if (cfg.logBuildInfo) {
            logBuildInfo();
        }

        std::optional<SingleInstanceGuard> instanceGuard{};
        if (cfg.singleInstanceEnabled) {
            instanceGuard.emplace();
            using enum SingleInstanceStatus;
            switch (instanceGuard->status()) {
                case AlreadyRunning: {
                    LOG_WARN(core::log::cat::Engine,
                             "Another instance is already running. Exiting.");
                    if (cfg.showErrorDialogOnSingleGuard) {
                        showError(cfg.alreadyRunningDialogText);
                    }
                    if (cfg.holdOnExit) {
                        holdOnExitIfEnabled();
                    }
                    return EXIT_SUCCESS;
                }
                case OsError: {
                    LOG_ERROR(core::log::cat::Engine,
                              "Single-instance lock acquisition failed (OS error).");
                    if (cfg.showErrorDialogOnSingleGuard) {
                        showError(cfg.singleGuardOsErrorDialogText);
                    }
                    if (cfg.singleInstanceIsFatal) {
                        if (cfg.holdOnExit) {
                            holdOnExitIfEnabled();
                        }
                        return EXIT_FAILURE;
                    }
                    break;
                }
                case Acquired: {
                    break;
                }
                case Inactive: {
                    std::abort();
                }
            }
        }

        try {
            if constexpr (std::is_void_v<BodyResult>) {
                std::forward<Body>(body)();
                if (cfg.holdOnExit) {
                    holdOnExitIfEnabled();
                }
                return EXIT_SUCCESS;
            } else {
                const int code = std::forward<Body>(body)();
                if (cfg.holdOnExit) {
                    holdOnExitIfEnabled();
                }
                return code;
            }
        } catch (const std::exception& e) {
            LOG_ERROR(core::log::cat::Engine,
                      "Unhandled exception in entry body: {}",
                      e.what());
            if (cfg.exceptionPrinter != nullptr) {
                cfg.exceptionPrinter(e.what());
            }
            if (cfg.showErrorDialogOnException) {
                showError(cfg.exceptionDialogText);
            }
            if (cfg.holdOnExit) {
                holdOnExitIfEnabled();
            }
            return EXIT_FAILURE;
        } catch (...) {
            LOG_ERROR(core::log::cat::Engine,
                      "Unhandled unknown exception in entry body.");
            if (cfg.unknownExceptionPrinter != nullptr) {
                cfg.unknownExceptionPrinter(cfg.unknownExceptionDialogText);
            }
            if (cfg.showErrorDialogOnException) {
                showError(cfg.unknownExceptionDialogText);
            }
            if (cfg.holdOnExit) {
                holdOnExitIfEnabled();
            }
            return EXIT_FAILURE;
        }
    }

} // namespace core::runtime::entry