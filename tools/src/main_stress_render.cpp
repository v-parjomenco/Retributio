#include "pch.h"

#include <array>
#include <cstdlib>
#include <string_view>

#include "core/debug/scoped_timer.h"
#include "core/log/log_macros.h"
#include "core/runtime/entry/run_main.h"

#include "game.h"

namespace {

    [[nodiscard]] bool isEnvSet(const char* name) noexcept {
#ifdef _WIN32
        std::size_t required = 0;
        if (::getenv_s(&required, nullptr, 0, name) != 0) {
            return false;
        }
        return required > 1;
#else
        const char* s = std::getenv(name);
        return s != nullptr && *s != '\0';
#endif
    }

    void setEnvKV(const char* key, const char* value) noexcept {
#ifdef _WIN32
        (void) ::_putenv_s(key, value);
#else
        (void) ::setenv(key, value, 1);
#endif
    }

    void prepareRenderStressEnv() noexcept {
        // Clear conflicting spatial namespace. If both are set, readStressModeFromEnv() panics.
        // Dedicated render exe must guarantee only render namespace is active.
        setEnvKV("ATRAPACIELOS_STRESS_SPATIAL_ENABLED", "0");

        if (!isEnvSet("ATRAPACIELOS_STRESS_RENDER_ENABLED")) {
            setEnvKV("ATRAPACIELOS_STRESS_RENDER_ENABLED", "1");
        }
        if (!isEnvSet("ATRAPACIELOS_STRESS_RENDER_OVERLAY_BACKPLATE")) {
            setEnvKV("ATRAPACIELOS_STRESS_RENDER_OVERLAY_BACKPLATE", "1");
        }

        LOG_INFO(core::log::cat::Performance,
                 "Render stress env prepared (spatial namespace cleared)");
    }

} // namespace

int main() {
    using namespace std::string_view_literals;

    static constexpr std::array sentinels{
        "assets/config/engine_settings.json"sv,
        "assets/config/atrapacielos.json"sv,
    };

    core::runtime::entry::EntryConfig cfg{};
    cfg.sentinels = sentinels;
    cfg.workingDirIsFatal = false;
    cfg.singleInstanceEnabled = false;
    cfg.logBuildInfo = true;
    cfg.holdOnExit = false;

    return core::runtime::entry::runMain(cfg, []() {
#if !defined(RETRIBUTIO_PROFILE)
        LOG_PANIC(core::log::cat::Engine, "Render stress is Profile-only. Build with -C Profile.");
#else
        prepareRenderStressEnv();

        LOG_INFO(core::log::cat::Engine,
                 "Запуск Atrapacielos render stress (real game path, Profile)...");

        core::debug::ScopedTimer timer{"Render stress session"};

        game::atrapacielos::Game game;
        game.run();

        LOG_INFO(core::log::cat::Engine, "Render stress завершён штатно.");
#endif
    });
}