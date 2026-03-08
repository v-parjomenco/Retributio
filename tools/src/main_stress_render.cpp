#include "pch.h"

#include <array>
#include <cstdlib>
#include <string>
#include <string_view>

#include "core/debug/scoped_timer.h"
#include "core/log/log_macros.h"
#include "core/runtime/entry/run_main.h"

#include "game.h"

namespace {

#ifdef _WIN32
    template <std::size_t N>
    [[nodiscard]] std::string_view readEnvStringView(const char* name,
                                                     std::array<char, N>& buf) noexcept {
        static_assert(N > 1, "Buffer must have room for null terminator.");
        std::size_t required = 0;
        if (::getenv_s(&required, buf.data(), buf.size(), name) != 0) {
            return {};
        }
        if (required == 0 || required > buf.size()) {
            return {};
        }
        return std::string_view{buf.data(), required - 1};
    }
#endif

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
    [[nodiscard]] std::string envValueOr(const char* name, std::string_view fallback) noexcept {
#ifdef _WIN32
        std::array<char, 256> buf{};
        std::size_t required = 0;
        if (::getenv_s(&required, buf.data(), buf.size(), name) != 0) {
            return std::string(fallback);
        }
        if (required <= 1 || required > buf.size()) {
            return std::string(fallback);
        }
        return std::string(buf.data(), required - 1);
#else
        const char* v = std::getenv(name);
        if (v == nullptr || *v == '\0') {
            return std::string(fallback);
        }
        return std::string(v);
#endif
    }

    void setEnvKV(const char* key, const char* value) noexcept {
#ifdef _WIN32
        (void) ::_putenv_s(key, value);
#else
        (void) ::setenv(key, value, 1);
#endif
    }

    void mapRenderStressEnvToSpatialStress() noexcept {
        struct EnvMap {
            const char* renderKey;
            const char* spatialKey;
        };

        static constexpr std::array kMap{
            EnvMap{"ATRAPACIELOS_STRESS_RENDER_ENABLED", "ATRAPACIELOS_STRESS_SPATIAL_ENABLED"},
            EnvMap{"ATRAPACIELOS_STRESS_RENDER_SEED", "ATRAPACIELOS_STRESS_SPATIAL_SEED"},
            EnvMap{"ATRAPACIELOS_STRESS_RENDER_ENTITIES_PER_CHUNK",
                   "ATRAPACIELOS_STRESS_SPATIAL_ENTITIES_PER_CHUNK"},
            EnvMap{"ATRAPACIELOS_STRESS_RENDER_TEXTURE_COUNT",
                   "ATRAPACIELOS_STRESS_SPATIAL_TEXTURE_COUNT"},
            EnvMap{"ATRAPACIELOS_STRESS_RENDER_TEXTURE_IDS",
                   "ATRAPACIELOS_STRESS_SPATIAL_TEXTURE_IDS"},
            EnvMap{"ATRAPACIELOS_STRESS_RENDER_Z_LAYERS", "ATRAPACIELOS_STRESS_SPATIAL_Z_LAYERS"},
            EnvMap{"ATRAPACIELOS_STRESS_RENDER_WINDOW_WIDTH",
                   "ATRAPACIELOS_STRESS_SPATIAL_WINDOW_WIDTH"},
            EnvMap{"ATRAPACIELOS_STRESS_RENDER_WINDOW_HEIGHT",
                   "ATRAPACIELOS_STRESS_SPATIAL_WINDOW_HEIGHT"},
        };

        for (const auto& m : kMap) {
#ifdef _WIN32
            std::array<char, 256> buf{};
            const std::string_view value = readEnvStringView(m.renderKey, buf);
            if (value.empty()) {
                continue;
            }
            setEnvKV(m.spatialKey, std::string(value).c_str());
#else
            const char* value = std::getenv(m.renderKey);
            if (value == nullptr || *value == '\0') {
                continue;
            }
            setEnvKV(m.spatialKey, value);
#endif
        }

        if (!isEnvSet("ATRAPACIELOS_STRESS_SPATIAL_ENABLED")) {
            setEnvKV("ATRAPACIELOS_STRESS_SPATIAL_ENABLED", "1");
        }

        if (!isEnvSet("ATRAPACIELOS_STRESS_SPATIAL_ENTITIES_PER_CHUNK")) {
            setEnvKV("ATRAPACIELOS_STRESS_SPATIAL_ENTITIES_PER_CHUNK", "64");
        }
        if (!isEnvSet("ATRAPACIELOS_STRESS_SPATIAL_WINDOW_WIDTH")) {
            setEnvKV("ATRAPACIELOS_STRESS_SPATIAL_WINDOW_WIDTH", "125");
        }
        if (!isEnvSet("ATRAPACIELOS_STRESS_SPATIAL_WINDOW_HEIGHT")) {
            setEnvKV("ATRAPACIELOS_STRESS_SPATIAL_WINDOW_HEIGHT", "125");
        }
        if (!isEnvSet("ATRAPACIELOS_STRESS_SPATIAL_TEXTURE_COUNT")) {
            setEnvKV("ATRAPACIELOS_STRESS_SPATIAL_TEXTURE_COUNT", "1");
        }

        if (!isEnvSet("ATRAPACIELOS_STRESS_RENDER_OVERLAY_BACKPLATE")) {
            setEnvKV("ATRAPACIELOS_STRESS_RENDER_OVERLAY_BACKPLATE", "1");
        }

        const std::string perChunk =
            envValueOr("ATRAPACIELOS_STRESS_SPATIAL_ENTITIES_PER_CHUNK", "<unset>");
        const std::string windowW =
            envValueOr("ATRAPACIELOS_STRESS_SPATIAL_WINDOW_WIDTH", "<unset>");
        const std::string windowH =
            envValueOr("ATRAPACIELOS_STRESS_SPATIAL_WINDOW_HEIGHT", "<unset>");
        const std::string texCount =
            envValueOr("ATRAPACIELOS_STRESS_SPATIAL_TEXTURE_COUNT", "<unset>");
        const std::string panel =
            envValueOr("ATRAPACIELOS_STRESS_RENDER_OVERLAY_BACKPLATE", "<unset>");

        LOG_INFO(
            core::log::cat::Performance,
            "Render stress env resolved: perChunk={} window={}x{} texCount={} overlayBackplate={}",
            perChunk, windowW, windowH, texCount, panel);
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
        mapRenderStressEnvToSpatialStress();

        LOG_INFO(core::log::cat::Engine, "Запуск Atrapacielos render stress (interactive Profile)...");

        core::debug::ScopedTimer timer{"Render stress session"};
        game::atrapacielos::Game game;
        game.run();

        LOG_INFO(core::log::cat::Engine, "Render stress завершён штатно.");
#endif
    });
}