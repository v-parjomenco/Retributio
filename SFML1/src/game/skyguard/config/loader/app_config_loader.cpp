#include "pch.h"

#include "game/skyguard/config/loader/app_config_loader.h"

#include <array>
#include <cctype>
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
    namespace ak = game::skyguard::config::keys::App;

    using Json = json_utils::json;
    using Report = core::config::loader::detail::NonCriticalConfigReport;

    static constexpr std::string_view kLoaderTag = "SkyGuard::AppConfigLoader";
    static constexpr std::string_view kKnownKeysHint = "app.id/app.display_name";

    static constexpr std::array kKnownKeys{ak::ID, ak::DISPLAY_NAME};

    static_assert(kKnownKeys.size() <= Report::kMaxFields,
                  "NonCriticalConfigReport buffer too small for AppConfigLoader");

    [[nodiscard]] bool hasAnyKnownKey(const Json& data) noexcept {
        for (const std::string_view key : kKnownKeys) {
            if (data.find(key) != data.end()) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool isSafeAppId(std::string_view s) noexcept {
        // Разрешаем только path-safe символы: [a-zA-Z0-9._-], без пробелов и слэшей.
        if (s.empty() || s.size() > 64u) {
            return false;
        }

        for (const unsigned char ch : s) {
            if (std::isalnum(ch) != 0) {
                continue;
            }
            if (ch == '.' || ch == '_' || ch == '-') {
                continue;
            }
            return false;
        }
        return true;
    }

} // namespace

namespace game::skyguard::config {

    AppConfig loadAppConfig(const std::string& path) {
        AppConfig cfg{};

        const auto fileContentOpt = FileLoader::loadTextFile(path);
        if (!fileContentOpt) {
            LOG_WARN(core::log::cat::Config,
                     "[{}] Файл не найден или не читается: '{}'. "
                     "Используются defaults (id='{}', display_name='{}').",
                     kLoaderTag,
                     path,
                     cfg.id,
                     cfg.displayName);
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
                     "Используются defaults (id='{}', display_name='{}'). "
                     "Подробности — DEBUG.",
                     kLoaderTag,
                     path,
                     cfg.id,
                     cfg.displayName);
            return cfg;
        }

        const Json& data = *dataOpt;

        if (!data.is_object()) {
            LOG_WARN(core::log::cat::Config,
                     "[{}] Корневой JSON в '{}' должен быть object. "
                     "Используются defaults (id='{}', display_name='{}').",
                     kLoaderTag,
                     path,
                     cfg.id,
                     cfg.displayName);
            return cfg;
        }

        const auto appIt = data.find(gk::APP);
        if (appIt == data.end() || !appIt->is_object()) {
            LOG_WARN(core::log::cat::Config,
                     "[{}] Отсутствует блок '{}' в '{}' (или он не object). "
                     "Используются defaults (id='{}', display_name='{}').",
                     kLoaderTag,
                     gk::APP,
                     path,
                     cfg.id,
                     cfg.displayName);
            return cfg;
        }

        const Json& appData = *appIt;

        Report report{};

        if (!hasAnyKnownKey(appData)) {
            report.emitWarnNoKnownKeys(kLoaderTag, path, kKnownKeysHint);
            return cfg;
        }

        // app.id
        {
            const auto it = appData.find(ak::ID);
            if (it != appData.end()) {
                if (it->is_string()) {
                    const std::string_view s = it->get_ref<const std::string&>();
                    if (isSafeAppId(s)) {
                        cfg.id.assign(s.data(), s.size());
                    } else {
                        report.addSemanticInvalidField(ak::ID);
                    }
                } else {
                    report.addInvalidField(ak::ID);
                }
            }
        }

        // app.display_name
        {
            const auto it = appData.find(ak::DISPLAY_NAME);
            if (it != appData.end()) {
                if (it->is_string()) {
                    const std::string_view s = it->get_ref<const std::string&>();
                    if (!s.empty()) {
                        cfg.displayName.assign(s.data(), s.size());
                    } else {
                        report.addSemanticInvalidField(ak::DISPLAY_NAME);
                    }
                } else {
                    report.addInvalidField(ak::DISPLAY_NAME);
                }
            }
        }

        if (report.hasAnyIssues()) {
            report.emitWarnPartialApply(kLoaderTag, path, kKnownKeysHint);
        }

        return cfg;
    }

} // namespace game::skyguard::config
