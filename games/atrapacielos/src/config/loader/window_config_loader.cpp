#include "pch.h"

#include "config/loader/window_config_loader.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "core/config/loader/detail/non_critical_config_report.h"
#include "core/log/log_macros.h"
#include "core/utils/file_loader.h"
#include "core/utils/json/json_document.h"
#include "core/utils/json/json_parsers.h"

#include "config/config_keys.h"

namespace {

    using core::utils::FileLoader;

    namespace json_utils = core::utils::json;
    namespace gk = game::atrapacielos::config::keys::Game;
    namespace wk = game::atrapacielos::config::keys::Window;

    using Json = json_utils::json;
    using Report = core::config::loader::detail::NonCriticalConfigReport;

    static constexpr std::string_view kLoaderTag = "Atrapacielos::WindowConfigLoader";
    static constexpr std::string_view kKnownKeysHint =
        "window.mode/window.width/window.height";

    static constexpr std::array kKnownKeys{wk::MODE, wk::WIDTH, wk::HEIGHT};

    static_assert(kKnownKeys.size() <= Report::kMaxFields,
                  "NonCriticalConfigReport buffer too small for WindowConfigLoader");

    [[nodiscard]] bool hasAnyKnownKey(const Json& data) noexcept {
        for (const std::string_view key : kKnownKeys) {
            if (data.find(key) != data.end()) {
                return true;
            }
        }
        return false;
    }

} // namespace

namespace game::atrapacielos::config {

    WindowConfig loadWindowConfig(const std::string& path) {
        WindowConfig cfg{};

        const auto fileContentOpt = FileLoader::loadTextFile(path);
        if (!fileContentOpt) {
            LOG_WARN(core::log::cat::Config,
                     "[{}] Файл не найден или не читается: '{}'. "
                     "Используются значения по умолчанию (mode={}, width={}, height={}).",
                     kLoaderTag,
                     path,
                     static_cast<int>(cfg.mode),
                     cfg.width,
                     cfg.height);
            return cfg;
        }

        const auto dataOpt = json_utils::parseAndValidateNonCritical(
            *fileContentOpt,
            path,
            kLoaderTag,
            {},
            json_utils::kConfigParseOnlyOptions);

        if (!dataOpt) {
            LOG_WARN(core::log::cat::Config,
                     "[{}] Не удалось разобрать JSON в '{}'. "
                     "Используются значения по умолчанию (mode={}, width={}, height={}). "
                     "Подробности — DEBUG.",
                     kLoaderTag,
                     path,
                     static_cast<int>(cfg.mode),
                     cfg.width,
                     cfg.height);
            return cfg;
        }

        const Json& data = *dataOpt;

        if (!data.is_object()) {
            LOG_WARN(core::log::cat::Config,
                     "[{}] Корневой JSON в '{}' должен быть object. "
                     "Используются значения по умолчанию (mode={}, width={}, height={}).",
                     kLoaderTag,
                     path,
                     static_cast<int>(cfg.mode),
                     cfg.width,
                     cfg.height);
            return cfg;
        }

        const auto windowIt = data.find(gk::WINDOW);
        if (windowIt == data.end()) {
            LOG_WARN(core::log::cat::Config,
                     "[{}] Отсутствует блок '{}' в '{}'. "
                     "Используются значения по умолчанию (mode={}, width={}, height={}).",
                     kLoaderTag,
                     gk::WINDOW,
                     path,
                     static_cast<int>(cfg.mode),
                     cfg.width,
                     cfg.height);
            return cfg;
        }

        if (!windowIt->is_object()) {
            LOG_WARN(core::log::cat::Config,
                     "[{}] Блок '{}' в '{}' должен быть object. "
                     "Используются значения по умолчанию (mode={}, width={}, height={}).",
                     kLoaderTag,
                     gk::WINDOW,
                     path,
                     static_cast<int>(cfg.mode),
                     cfg.width,
                     cfg.height);
            return cfg;
        }

        const Json& windowData = *windowIt;

        Report report{};

        if (!hasAnyKnownKey(windowData)) {
            report.emitWarnNoKnownKeys(kLoaderTag, path, kKnownKeysHint);
            return cfg;
        }

        // Режим окна
        {
            const WindowMode defaultMode = cfg.mode;

            const auto it = windowData.find(wk::MODE);
            if (it != windowData.end()) {
                if (it->is_string()) {
                    const std::string_view modeStr = it->get_ref<const std::string&>();

                    if (modeStr == "windowed") {
                        cfg.mode = WindowMode::Windowed;
                    } else if (modeStr == "borderless_fullscreen") {
                        cfg.mode = WindowMode::BorderlessFullscreen;
                    } else if (modeStr == "fullscreen") {
                        cfg.mode = WindowMode::Fullscreen;
                    } else {
                        report.addSemanticInvalidField(wk::MODE);
                        cfg.mode = defaultMode;
                    }
                } else {
                    report.addInvalidField(wk::MODE);
                }
            }
        }

        // Ширина
        {
            const std::uint32_t defaultWidth = cfg.width;

            const auto res =
                json_utils::parseUIntWithIssue<std::uint32_t>(windowData,
                                                             wk::WIDTH,
                                                             defaultWidth);

            cfg.width = res.value;
            core::config::loader::detail::noteUIntIssue(report, wk::WIDTH, res);

            // validate-on-write: 0 недопустим
            const bool hasKey = (windowData.find(wk::WIDTH) != windowData.end());
            using Kind = json_utils::UnsignedParseIssue::Kind;
            if (hasKey && res.issue.kind == Kind::None && cfg.width == 0u) {
                report.addSemanticInvalidField(wk::WIDTH);
                cfg.width = defaultWidth;
            }
        }

        // Высота
        {
            const std::uint32_t defaultHeight = cfg.height;

            const auto res =
                json_utils::parseUIntWithIssue<std::uint32_t>(windowData,
                                                             wk::HEIGHT,
                                                             defaultHeight);

            cfg.height = res.value;
            core::config::loader::detail::noteUIntIssue(report, wk::HEIGHT, res);

            // validate-on-write: 0 недопустим
            const bool hasKey = (windowData.find(wk::HEIGHT) != windowData.end());
            using Kind = json_utils::UnsignedParseIssue::Kind;
            if (hasKey && res.issue.kind == Kind::None && cfg.height == 0u) {
                report.addSemanticInvalidField(wk::HEIGHT);
                cfg.height = defaultHeight;
            }
        }

        if (report.hasAnyIssues()) {
            report.emitWarnPartialApply(kLoaderTag, path, kKnownKeysHint);
        }

        return cfg;
    }

} // namespace game::atrapacielos::config