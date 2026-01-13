#include "pch.h"

#include "game/skyguard/config/loader/view_config_loader.h"

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
    namespace vk = game::skyguard::config::keys::View;

    using Json = json_utils::json;
    using Report = core::config::loader::detail::NonCriticalConfigReport;

    static constexpr std::string_view kLoaderTag = "SkyGuard::ViewConfigLoader";
    static constexpr std::string_view kKnownKeysHint =
        "view.world_logical_size/view.ui_logical_size/view.camera_offset";

    static constexpr std::array kKnownKeys{
        vk::WORLD_LOGICAL_SIZE,
        vk::UI_LOGICAL_SIZE,
        vk::CAMERA_OFFSET
    };

    static_assert(kKnownKeys.size() <= Report::kMaxFields,
                  "NonCriticalConfigReport buffer too small for ViewConfigLoader");

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

    ViewConfig loadViewConfig(const std::string& path) {
        ViewConfig cfg{};

        const auto fileContentOpt = FileLoader::loadTextFile(path);
        if (!fileContentOpt) {
            LOG_WARN(core::log::cat::Config,
                     "[{}] Файл не найден или не читается: '{}'. "
                     "Используются значения по умолчанию.",
                     kLoaderTag,
                     path);
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
                     "Используются значения по умолчанию. Подробности — DEBUG.",
                     kLoaderTag,
                     path);
            return cfg;
        }

        const Json& data = *dataOpt;

        if (!data.is_object()) {
            LOG_WARN(core::log::cat::Config,
                     "[{}] Корневой JSON в '{}' должен быть object. "
                     "Используются значения по умолчанию.",
                     kLoaderTag,
                     path);
            return cfg;
        }

        const auto viewIt = data.find(gk::VIEW);
        if (viewIt == data.end()) {
            LOG_WARN(core::log::cat::Config,
                     "[{}] Отсутствует блок '{}' в '{}'. "
                     "Используются значения по умолчанию.",
                     kLoaderTag,
                     gk::VIEW,
                     path);
            return cfg;
        }

        if (!viewIt->is_object()) {
            LOG_WARN(core::log::cat::Config,
                     "[{}] Блок '{}' в '{}' должен быть object. "
                     "Используются значения по умолчанию.",
                     kLoaderTag,
                     gk::VIEW,
                     path);
            return cfg;
        }

        const Json& viewData = *viewIt;
        Report report{};

        if (!hasAnyKnownKey(viewData)) {
            report.emitWarnNoKnownKeys(kLoaderTag, path, kKnownKeysHint);
            return cfg;
        }

        const auto worldSizeRes =
            json_utils::parseVec2fWithIssue(viewData, vk::WORLD_LOGICAL_SIZE, cfg.worldLogicalSize);
        if (worldSizeRes.issue.kind == json_utils::Vec2ParseIssue::Kind::None) {
            cfg.worldLogicalSize = worldSizeRes.value;
            if (!(cfg.worldLogicalSize.x > 0.f) || !(cfg.worldLogicalSize.y > 0.f)) {
                report.addSemanticInvalidField(vk::WORLD_LOGICAL_SIZE);
                cfg.worldLogicalSize = ViewConfig{}.worldLogicalSize;
            }
        } else if (worldSizeRes.issue.kind != json_utils::Vec2ParseIssue::Kind::MissingKey) {
            report.addInvalidField(vk::WORLD_LOGICAL_SIZE);
        }

        const auto uiSizeRes =
            json_utils::parseVec2fWithIssue(viewData, vk::UI_LOGICAL_SIZE, cfg.uiLogicalSize);
        if (uiSizeRes.issue.kind == json_utils::Vec2ParseIssue::Kind::None) {
            cfg.uiLogicalSize = uiSizeRes.value;
            if (!(cfg.uiLogicalSize.x > 0.f) || !(cfg.uiLogicalSize.y > 0.f)) {
                report.addSemanticInvalidField(vk::UI_LOGICAL_SIZE);
                cfg.uiLogicalSize = ViewConfig{}.uiLogicalSize;
            }
        } else if (uiSizeRes.issue.kind != json_utils::Vec2ParseIssue::Kind::MissingKey) {
            report.addInvalidField(vk::UI_LOGICAL_SIZE);
        }

        const auto offsetRes =
            json_utils::parseVec2fWithIssue(viewData, vk::CAMERA_OFFSET, cfg.cameraOffset);
        if (offsetRes.issue.kind == json_utils::Vec2ParseIssue::Kind::None) {
            cfg.cameraOffset = offsetRes.value;
        } else if (offsetRes.issue.kind != json_utils::Vec2ParseIssue::Kind::MissingKey) {
            report.addInvalidField(vk::CAMERA_OFFSET);
        }

        if (report.hasAnyIssues()) {
            report.emitWarnPartialApply(kLoaderTag, path, kKnownKeysHint);
        }

        return cfg;
    }

} // namespace game::skyguard::config