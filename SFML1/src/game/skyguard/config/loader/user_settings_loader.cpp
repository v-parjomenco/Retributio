#include "pch.h"

#include "game/skyguard/config/loader/user_settings_loader.h"

#include <array>
#include <string>
#include <string_view>

#include "core/config/loader/detail/non_critical_config_report.h"
#include "core/log/log_macros.h"
#include "core/utils/file_loader.h"
#include "core/utils/json/json_document.h"
#include "core/utils/json/json_parsers.h"

#include "game/skyguard/config/config_keys.h"

namespace {

    using core::utils::FileLoader;

    namespace json_utils = core::utils::json;
    namespace gk = game::skyguard::config::keys::Game;
    namespace wk = game::skyguard::config::keys::Window;

    using Json = json_utils::json;
    using Report = core::config::loader::detail::NonCriticalConfigReport;

    static constexpr std::string_view kLoaderTag = "SkyGuard::UserSettingsLoader";
    static constexpr std::string_view kKnownKeysHint =
        "window.mode/window.width/window.height";

    static constexpr std::array kKnownKeys{wk::MODE, wk::WIDTH, wk::HEIGHT};

    static_assert(kKnownKeys.size() <= Report::kMaxFields,
                  "NonCriticalConfigReport buffer too small for UserSettingsLoader");

    [[nodiscard]] bool hasAnyKnownKey(const Json& data) noexcept {
        for (const std::string_view key : kKnownKeys) {
            if (data.find(key) != data.end()) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] std::string_view toModeString(game::skyguard::config::WindowMode mode) noexcept {
        using WM = game::skyguard::config::WindowMode;
        switch (mode) {
            case WM::Windowed:             return "windowed";
            case WM::BorderlessFullscreen: return "borderless_fullscreen";
            case WM::Fullscreen:           return "fullscreen";
        }
        return "borderless_fullscreen";
    }

    [[nodiscard]] std::optional<game::skyguard::config::WindowMode> parseModeString(
        const std::string_view s) noexcept {
        using WM = game::skyguard::config::WindowMode;

        if (s == "windowed") {
            return WM::Windowed;
        }
        if (s == "borderless_fullscreen") {
            return WM::BorderlessFullscreen;
        }
        if (s == "fullscreen") {
            return WM::Fullscreen;
        }
        return std::nullopt;
    }

} // namespace

namespace game::skyguard::config {

    UserSettings loadUserSettings(const std::filesystem::path& path) {
        UserSettings settings{};

        const std::string pathStr = path.string();

        // Отсутствие файла — нормальный кейс, без WARN.
        if (!FileLoader::fileExists(path)) {
            return settings;
        }

        const auto fileContentOpt = FileLoader::loadTextFile(path);
        if (!fileContentOpt) {
            // Файл существует, но не читается: один WARN на запуск, без спама.
            LOG_WARN(core::log::cat::Config,
                     "[{}] Файл user settings существует, но не читается: '{}'. "
                     "Переходим без override.",
                     kLoaderTag,
                     pathStr);
            return settings;
        }

        const auto dataOpt = json_utils::parseAndValidateNonCritical(
            *fileContentOpt,
            pathStr,
            kLoaderTag,
            {},
            json_utils::kConfigParseOnlyOptions);

        if (!dataOpt) {
            // parseAndValidateNonCritical пишет подробности в DEBUG, здесь — один понятный WARN.
            LOG_WARN(core::log::cat::Config,
                     "[{}] Не удалось разобрать user settings JSON: '{}'. "
                     "Переходим без override. Подробности — DEBUG.",
                     kLoaderTag,
                     pathStr);
            return settings;
        }

        const Json& data = *dataOpt;

        if (!data.is_object()) {
            return settings;
        }

        const auto windowIt = data.find(gk::WINDOW);
        if (windowIt == data.end() || !windowIt->is_object()) {
            return settings;
        }

        const Json& windowData = *windowIt;

        Report report{};
        if (!hasAnyKnownKey(windowData)) {
            report.emitWarnNoKnownKeys(kLoaderTag, pathStr, kKnownKeysHint);
            return settings;
        }

        // mode
        {
            const auto it = windowData.find(wk::MODE);
            if (it != windowData.end()) {
                if (it->is_string()) {
                    const std::string_view modeStr = it->get_ref<const std::string&>();
                    const auto parsed = parseModeString(modeStr);
                    if (parsed) {
                        settings.window.mode = *parsed;
                    } else {
                        report.addSemanticInvalidField(wk::MODE);
                    }
                } else {
                    report.addInvalidField(wk::MODE);
                }
            }
        }

        // width
        {
            const auto it = windowData.find(wk::WIDTH);
            if (it != windowData.end()) {
                const auto res =
                    json_utils::parseUIntWithIssue<std::uint32_t>(windowData, wk::WIDTH, 0u);

                core::config::loader::detail::noteUIntIssue(report, wk::WIDTH, res);

                using Kind = json_utils::UnsignedParseIssue::Kind;
                if (res.issue.kind == Kind::None && res.value != 0u) {
                    settings.window.width = res.value;
                } else if (res.issue.kind == Kind::None && res.value == 0u) {
                    report.addSemanticInvalidField(wk::WIDTH);
                }
            }
        }

        // height
        {
            const auto it = windowData.find(wk::HEIGHT);
            if (it != windowData.end()) {
                const auto res =
                    json_utils::parseUIntWithIssue<std::uint32_t>(windowData, wk::HEIGHT, 0u);

                core::config::loader::detail::noteUIntIssue(report, wk::HEIGHT, res);

                using Kind = json_utils::UnsignedParseIssue::Kind;
                if (res.issue.kind == Kind::None && res.value != 0u) {
                    settings.window.height = res.value;
                } else if (res.issue.kind == Kind::None && res.value == 0u) {
                    report.addSemanticInvalidField(wk::HEIGHT);
                }
            }
        }

        if (report.hasAnyIssues()) {
            report.emitWarnPartialApply(kLoaderTag, pathStr, kKnownKeysHint);
        }

        return settings;
    }

    bool saveUserSettingsAtomic(const std::filesystem::path& path,
                                const UserSettings& settings) noexcept {
        const bool hasAnything =
            settings.window.mode.has_value() ||
            settings.window.width.has_value() ||
            settings.window.height.has_value();

        if (!hasAnything) {
            return true; // no-op
        }

        Json root = Json::object();
        Json window = Json::object();

        if (settings.window.mode) {
            window[wk::MODE] = std::string(toModeString(*settings.window.mode));
        }
        if (settings.window.width) {
            window[wk::WIDTH] = *settings.window.width;
        }
        if (settings.window.height) {
            window[wk::HEIGHT] = *settings.window.height;
        }

        root[gk::WINDOW] = std::move(window);

        const std::string text = root.dump(4);
        return FileLoader::writeTextFileAtomic(path, text);
    }

    WindowConfig applyUserSettings(const WindowConfig& base, const UserSettings& settings) {
        WindowConfig cfg = base;

        if (settings.window.mode) {
            cfg.mode = *settings.window.mode;
        }

        // width/height — это "desired windowed client size". Храним и применяем всегда:
        // borderless/fullscreen их игнорируют, а при переключении обратно в windowed они нужны.
        if (settings.window.width) {
            cfg.width = *settings.window.width;
        }
        if (settings.window.height) {
            cfg.height = *settings.window.height;
        }

        return cfg;
    }

    bool setWindowMode(UserSettings& settings, const WindowMode mode) noexcept {
        if (settings.window.mode && *settings.window.mode == mode) {
            return false;
        }
        settings.window.mode = mode;
        return true;
    }

    bool setWindowedSize(UserSettings& settings,
                         const std::uint32_t width,
                         const std::uint32_t height) noexcept {
        // validate-on-write: 0 запрещён
        if (width == 0u || height == 0u) {
            return false;
        }

        bool changed = false;

        if (!settings.window.width || *settings.window.width != width) {
            settings.window.width = width;
            changed = true;
        }
        if (!settings.window.height || *settings.window.height != height) {
            settings.window.height = height;
            changed = true;
        }

        return changed;
    }

} // namespace game::skyguard::config
