#include "pch.h"

#include "game/skyguard/config/loader/window_config_loader.h"

#include <array>
#include <cstdint>
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

    using Json = json_utils::json;
    using Report = core::config::loader::detail::NonCriticalConfigReport;

    static constexpr std::string_view kLoaderTag = "SkyGuard::WindowConfigLoader";
    static constexpr std::string_view kKnownKeysHint = "windowWidth/windowHeight/windowTitle";

    static constexpr std::array kKnownKeys{gk::WINDOW_WIDTH, gk::WINDOW_HEIGHT, gk::WINDOW_TITLE};

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

namespace game::skyguard::config {

    WindowConfig loadWindowConfig(const std::string& path) {
        WindowConfig cfg{};

        const auto fileContentOpt = FileLoader::loadTextFile(path);
        if (!fileContentOpt) {
            LOG_WARN(core::log::cat::Config,
                     "[{}] Файл не найден или не читается: '{}'. "
                     "Используются значения по умолчанию (width={}, height={}, title='{}').",
                     kLoaderTag,
                     path,
                     cfg.width,
                     cfg.height,
                     cfg.title);
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
                     "Используются значения по умолчанию (width={}, height={}, title='{}'). "
                     "Подробности — DEBUG.",
                     kLoaderTag,
                     path,
                     cfg.width,
                     cfg.height,
                     cfg.title);
            return cfg;
        }

        const Json& data = *dataOpt;

        if (!data.is_object()) {
            LOG_WARN(core::log::cat::Config,
                     "[{}] Корневой JSON в '{}' должен быть object. "
                     "Используются значения по умолчанию (width={}, height={}, title='{}').",
                     kLoaderTag,
                     path,
                     cfg.width,
                     cfg.height,
                     cfg.title);
            return cfg;
        }

        Report report{};

        if (!hasAnyKnownKey(data)) {
            report.emitWarnNoKnownKeys(kLoaderTag, path, kKnownKeysHint);
            return cfg;
        }

        // width
        {
            const std::uint32_t defaultWidth = cfg.width;

            const auto res =
                json_utils::parseUIntWithIssue<std::uint32_t>(data, gk::WINDOW_WIDTH, defaultWidth);

            cfg.width = res.value;
            core::config::loader::detail::noteUIntIssue(report, gk::WINDOW_WIDTH, res);

            // validate-on-write: 0 недопустим
            const bool hasKey = (data.find(gk::WINDOW_WIDTH) != data.end());
            using Kind = json_utils::UnsignedParseIssue::Kind;
            if (hasKey && res.issue.kind == Kind::None && cfg.width == 0u) {
                report.addSemanticInvalidField(gk::WINDOW_WIDTH);
                cfg.width = defaultWidth;
            }
        }

        // height
        {
            const std::uint32_t defaultHeight = cfg.height;

            const auto res =
                json_utils::parseUIntWithIssue<std::uint32_t>(data, gk::WINDOW_HEIGHT, defaultHeight);

            cfg.height = res.value;
            core::config::loader::detail::noteUIntIssue(report, gk::WINDOW_HEIGHT, res);

            // validate-on-write: 0 недопустим
            const bool hasKey = (data.find(gk::WINDOW_HEIGHT) != data.end());
            using Kind = json_utils::UnsignedParseIssue::Kind;
            if (hasKey && res.issue.kind == Kind::None && cfg.height == 0u) {
                report.addSemanticInvalidField(gk::WINDOW_HEIGHT);
                cfg.height = defaultHeight;
            }
        }

        // title
        {
            const auto it = data.find(gk::WINDOW_TITLE);
            if (it != data.end()) {
                if (it->is_string()) {
                    cfg.title = it->get_ref<const std::string&>();
                } else {
                    report.addInvalidField(gk::WINDOW_TITLE);
                }
            }
        }

        if (report.hasAnyIssues()) {
            report.emitWarnPartialApply(kLoaderTag, path, kKnownKeysHint);
        }

        return cfg;
    }

} // namespace game::skyguard::config