#include "pch.h"

#include "core/config/loader/engine_settings_loader.h"
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "core/config/config_keys.h"
#include "core/config/loader/detail/non_critical_config_report.h"
#include "core/log/log_macros.h"
#include "core/utils/file_loader.h"
#include "core/utils/json/json_document.h"
#include "core/utils/json/json_parsers.h"

namespace {

    using core::utils::FileLoader;

    namespace json_utils = core::utils::json;
    namespace eng_keys = core::config::keys::EngineSettings;

    using Json = json_utils::json;
    using Report = core::config::loader::detail::NonCriticalConfigReport;

    static constexpr std::string_view kLoaderTag = "EngineSettingsLoader";
    static constexpr std::string_view kKnownKeysHint = "vsync/frameLimit/spatialCellSize";

    static constexpr std::array kKnownKeys{
        eng_keys::VSYNC,
        eng_keys::FRAME_LIMIT,
        eng_keys::SPATIAL_CELL_SIZE
    };

    static_assert(kKnownKeys.size() <= Report::kMaxFields,
                  "NonCriticalConfigReport buffer too small for EngineSettingsLoader");

    [[nodiscard]] bool hasAnyKnownKey(const Json& data) noexcept {
        for (const std::string_view key : kKnownKeys) {
            if (data.find(key) != data.end()) {
                return true;
            }
        }
        return false;
    }

} // namespace

namespace core::config {

    EngineSettings loadEngineSettings(const std::string& path) {
        EngineSettings cfg{};

        const auto fileContentOpt = FileLoader::loadTextFile(path);
        if (!fileContentOpt) {
            LOG_WARN(core::log::cat::Config,
                     "[{}] Файл настроек движка не найден или не читается: '{}'. "
                     "Используются значения по умолчанию (vsyncEnabled={}, frameLimit={}, "
                     "spatialCellSize={}).",
                     kLoaderTag, path, cfg.vsyncEnabled, cfg.frameLimit, cfg.spatialCellSize);
            return cfg;
        }

        const auto dataOpt = json_utils::parseAndValidateNonCritical(
            *fileContentOpt, path, kLoaderTag, {}, json_utils::kConfigParseOnlyOptions);

        if (!dataOpt) {
            LOG_WARN(core::log::cat::Config,
                     "[{}] Не удалось разобрать JSON в '{}'. "
                     "Используются значения по умолчанию (vsyncEnabled={}, frameLimit={}, "
                     "spatialCellSize={}). Подробности — DEBUG.",
                     kLoaderTag, path, cfg.vsyncEnabled, cfg.frameLimit, cfg.spatialCellSize);
            return cfg;
        }

        const Json& data = *dataOpt;

        if (!data.is_object()) {
            LOG_WARN(core::log::cat::Config,
                     "[{}] Корневой JSON в '{}' должен быть object. "
                     "Используются значения по умолчанию (vsyncEnabled={}, frameLimit={}, "
                     "spatialCellSize={}).",
                     kLoaderTag, path, cfg.vsyncEnabled, cfg.frameLimit, cfg.spatialCellSize);
            return cfg;
        }

        Report report{};

        if (!hasAnyKnownKey(data)) {
            report.emitWarnNoKnownKeys(kLoaderTag, path, kKnownKeysHint);
            return cfg;
        }

        // vsyncEnabled (bool)
        {
            const auto it = data.find(eng_keys::VSYNC);
            if (it != data.end()) {
                if (it->is_boolean()) {
                    cfg.vsyncEnabled = it->get<bool>();
                } else {
                    report.addInvalidField(eng_keys::VSYNC);
                }
            }
        }

        // frameLimit (uint32_t)
        {
            const auto res = json_utils::parseUIntWithIssue<std::uint32_t>(
                data, eng_keys::FRAME_LIMIT, cfg.frameLimit);

            cfg.frameLimit = res.value;
            core::config::loader::detail::noteUIntIssue(report, eng_keys::FRAME_LIMIT, res);
        }

        // spatialCellSize (float)
        {
            const auto res = json_utils::parseFloatWithIssue(data, eng_keys::SPATIAL_CELL_SIZE,
                                                             cfg.spatialCellSize);

            using Kind = json_utils::FloatParseIssue::Kind;
            if (res.issue.kind == Kind::None) {
                if (res.value >= 64.f && res.value <= 2048.f) {
                    cfg.spatialCellSize = res.value;
                } else {
                    report.addSemanticInvalidField(eng_keys::SPATIAL_CELL_SIZE);
                }
            } else if (res.issue.kind != Kind::MissingKey) {
                report.addInvalidField(eng_keys::SPATIAL_CELL_SIZE);
            }
        }

        if (report.hasAnyIssues()) {
            report.emitWarnPartialApply(kLoaderTag, path, kKnownKeysHint);
        }

        return cfg;
    }

} // namespace core::config