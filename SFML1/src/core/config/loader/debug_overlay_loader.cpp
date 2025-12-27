#include "pch.h"

#include "core/config/loader/debug_overlay_loader.h"
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "core/config/config_keys.h"
#include "core/config/loader/detail/non_critical_config_report.h"
#include "core/config/properties/debug_overlay_runtime_properties.h"
#include "core/log/log_macros.h"
#include "core/utils/file_loader.h"
#include "core/utils/json/json_document.h"
#include "core/utils/json/json_parsers.h"

namespace {

    using core::utils::FileLoader;

    namespace keys = core::config::keys;
    namespace json_utils = core::utils::json;

    using Json = json_utils::json;
    using Report = core::config::loader::detail::NonCriticalConfigReport;

    static constexpr std::string_view kLoaderTag = "DebugOverlayLoader";

    static constexpr std::string_view kKnownKeysHint =
        "enabled/position/characterSize/color/updateIntervalMs/smoothingShift";

    static constexpr std::array kKnownKeys{keys::DebugOverlay::ENABLED,
                                           keys::DebugOverlay::POSITION,
                                           keys::DebugOverlay::CHARACTER_SIZE,
                                           keys::DebugOverlay::COLOR,
                                           keys::DebugOverlay::UPDATE_INTERVAL_MS,
                                           keys::DebugOverlay::SMOOTHING_SHIFT};

    static_assert(kKnownKeys.size() <= Report::kMaxFields,
                  "NonCriticalConfigReport buffer too small for DebugOverlayLoader");

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

    DebugOverlayBlueprint loadDebugOverlayBlueprint(const std::string& path) {
        DebugOverlayBlueprint cfg{};

        const auto fileContentOpt = FileLoader::loadTextFile(path);
        if (!fileContentOpt) {
            LOG_WARN(core::log::cat::Config,
                     "[{}] Не удалось прочитать '{}'. Используется конфигурация debug overlay "
                     "по умолчанию.",
                     kLoaderTag, path);
            return cfg;
        }

        const auto dataOpt = json_utils::parseAndValidateNonCritical(
            *fileContentOpt, path, kLoaderTag, {}, json_utils::kConfigParseOnlyOptions);

        if (!dataOpt) {
            LOG_WARN(core::log::cat::Config,
                     "[{}] Не удалось разобрать JSON в '{}'. Используется конфигурация debug "
                     "overlay по умолчанию. Подробности — DEBUG.",
                     kLoaderTag, path);
            return cfg;
        }

        const Json& data = *dataOpt;

        if (!data.is_object()) {
            LOG_WARN(core::log::cat::Config,
                     "[{}] Корневой JSON в '{}' должен быть object. Используется конфигурация "
                     "debug overlay по умолчанию.",
                     kLoaderTag, path);
            return cfg;
        }

        Report report{};

        if (!hasAnyKnownKey(data)) {
            report.emitWarnNoKnownKeys(kLoaderTag, path, kKnownKeysHint);
            return cfg;
        }

        // enabled (bool)
        {
            const auto it = data.find(keys::DebugOverlay::ENABLED);
            if (it != data.end()) {
                if (it->is_boolean()) {
                    cfg.enabled = it->get<bool>();
                } else {
                    report.addInvalidField(keys::DebugOverlay::ENABLED);
                }
            }
        }

        // position (Vector2f: number/array/object)
        {
            const auto res = json_utils::parseVec2fWithIssue(data, keys::DebugOverlay::POSITION,
                                                             cfg.text.position);

            cfg.text.position = res.value;

            using Kind = json_utils::Vec2ParseIssue::Kind;
            if (res.issue.kind != Kind::None && res.issue.kind != Kind::MissingKey) {
                report.addInvalidField(keys::DebugOverlay::POSITION);
            }
        }

        // characterSize (uint32_t)
        {
            const auto res = json_utils::parseUIntWithIssue<std::uint32_t>(
                data, keys::DebugOverlay::CHARACTER_SIZE, cfg.text.characterSize);

            cfg.text.characterSize = res.value;
            core::config::loader::detail::noteUIntIssue(report, keys::DebugOverlay::CHARACTER_SIZE,
                                                        res);
        }

        // color (#RRGGBB/#RRGGBBAA or {r,g,b,a})
        {
            const auto res =
                json_utils::parseColorWithIssue(data, keys::DebugOverlay::COLOR, cfg.text.color);

            cfg.text.color = res.value;

            using Kind = json_utils::ColorParseIssue::Kind;
            if (res.issue.kind != Kind::None && res.issue.kind != Kind::MissingKey) {
                report.addInvalidField(keys::DebugOverlay::COLOR);
            }
        }

        // updateIntervalMs (0 = каждый кадр, иначе throttle)
        {
            const auto res = json_utils::parseUIntWithIssue<std::uint32_t>(
                data, keys::DebugOverlay::UPDATE_INTERVAL_MS, cfg.runtime.updateIntervalMs);

            cfg.runtime.updateIntervalMs = res.value;
            core::config::loader::detail::noteUIntIssue(
                report, keys::DebugOverlay::UPDATE_INTERVAL_MS, res);

            if (res.issue.kind == json_utils::UnsignedParseIssue::Kind::None) {
                if (cfg.runtime.updateIntervalMs >
                    core::config::properties::DebugOverlayRuntimeProperties::kMaxUpdateIntervalMs) {

                    report.addClampedField(keys::DebugOverlay::UPDATE_INTERVAL_MS);
                    cfg.runtime.updateIntervalMs = core::config::properties::
                        DebugOverlayRuntimeProperties::kMaxUpdateIntervalMs;
                }
            }
        }

        // smoothingShift (0 = no smoothing)
        {
            const auto res = json_utils::parseUIntWithIssue<std::uint8_t>(
                data, keys::DebugOverlay::SMOOTHING_SHIFT, cfg.runtime.smoothingShift);

            cfg.runtime.smoothingShift = res.value;
            core::config::loader::detail::noteUIntIssue(report, keys::DebugOverlay::SMOOTHING_SHIFT,
                                                        res);

            if (res.issue.kind == json_utils::UnsignedParseIssue::Kind::None) {
                if (cfg.runtime.smoothingShift >
                    core::config::properties::DebugOverlayRuntimeProperties::kMaxSmoothingShift) {

                    report.addClampedField(keys::DebugOverlay::SMOOTHING_SHIFT);
                    cfg.runtime.smoothingShift =
                        core::config::properties::DebugOverlayRuntimeProperties::kMaxSmoothingShift;
                }
            }
        }

        if (report.hasAnyIssues()) {
            report.emitWarnPartialApply(kLoaderTag, path, kKnownKeysHint);
        }

        return cfg;
    }

} // namespace core::config