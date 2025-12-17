#include "pch.h"

#include "game/skyguard/config/loader/window_config_loader.h"

#include "core/log/log_macros.h"
#include "core/utils/file_loader.h"
#include "core/utils/json/json_accessors.h"
#include "core/utils/json/json_document.h"
#include "core/utils/json/json_parsers.h"

#include "game/skyguard/config/config_keys.h"

namespace {

    using core::utils::FileLoader;

    namespace json_utils = core::utils::json;
    namespace gk         = game::skyguard::config::keys::Game;

    using Json = json_utils::json;

} // namespace

namespace game::skyguard::config {

    WindowConfig loadWindowConfig(const std::string& path) {
        // Стартуем с безопасных дефолтов (источник истины №1 для значений по умолчанию).
        WindowConfig cfg{};

        // ----------------------------------------------------------------------------------------
        // Низкоуровневое чтение файла
        // ----------------------------------------------------------------------------------------
        const auto fileContentOpt = FileLoader::loadTextFile(path);
        if (!fileContentOpt) {
            LOG_WARN(core::log::cat::Config,
                     "[SkyGuard::WindowConfigLoader] Файл не найден или не читается: '{}'. "
                     "Используются значения по умолчанию (width={}, height={}, title='{}').",
                     path,
                     cfg.width,
                     cfg.height,
                     cfg.title);
            return cfg;
        }

        const std::string& fileContent = *fileContentOpt;

        // ----------------------------------------------------------------------------------------
        // Парсинг + валидация как НЕ критичного конфига.
        // ----------------------------------------------------------------------------------------
        const auto dataOpt = json_utils::parseAndValidateNonCritical(
            fileContent,
            path,
            "SkyGuard::WindowConfigLoader",
            {
                {gk::WINDOW_WIDTH,
                 {Json::value_t::number_integer, Json::value_t::number_unsigned},
                 false},
                {gk::WINDOW_HEIGHT,
                 {Json::value_t::number_integer, Json::value_t::number_unsigned},
                 false},
                {gk::WINDOW_TITLE, {Json::value_t::string}, false},
            });

        if (!dataOpt) {
            LOG_WARN(core::log::cat::Config,
                     "[SkyGuard::WindowConfigLoader] Некорректный JSON или структура в '{}'. "
                     "Используются значения по умолчанию (width={}, height={}, title='{}'). "
                     "Подробности — в логах уровня DEBUG (если включены).",
                     path,
                     cfg.width,
                     cfg.height,
                     cfg.title);
            return cfg;
        }

        const Json& data = *dataOpt;

        // ----------------------------------------------------------------------------------------
        // Заполнение WindowConfig из JSON (fallback на значения по умолчанию)
        // ----------------------------------------------------------------------------------------

        const bool hasWidth =
            (data.find(gk::WINDOW_WIDTH) != data.end());
        const bool hasHeight =
            (data.find(gk::WINDOW_HEIGHT) != data.end());
        const bool hasTitle =
            (data.find(gk::WINDOW_TITLE) != data.end());

        if (!hasWidth && !hasHeight && !hasTitle) {
            LOG_WARN(core::log::cat::Config,
                     "[SkyGuard::WindowConfigLoader] Конфиг '{}' прочитан, но не содержит "
                     "ни одного известного ключа (windowWidth/windowHeight/windowTitle). "
                     "Используются значения по умолчанию.",
                     path);
            return cfg;
        }

        {
            const unsigned defaultWidth = cfg.width;
            const auto res =
                json_utils::parseUnsignedWithIssue(data, gk::WINDOW_WIDTH, defaultWidth);

            cfg.width = res.value;

            using Kind = json_utils::UnsignedParseIssue::Kind;
            if (res.issue.kind != Kind::None && res.issue.kind != Kind::MissingKey) {

                if (res.issue.kind == Kind::Negative) {
                    LOG_WARN(core::log::cat::Config,
                             "[SkyGuard::WindowConfigLoader] Некорректное поле '{}': "
                             "значение отрицательное ({}). Применён дефолт ({}).",
                             gk::WINDOW_WIDTH,
                             res.rawSigned,
                             defaultWidth);
                } else if (res.issue.kind == Kind::OutOfRange) {
                    LOG_WARN(core::log::cat::Config,
                             "[SkyGuard::WindowConfigLoader] Некорректное поле '{}': "
                             "значение вне диапазона ({}). Применён дефолт ({}).",
                             gk::WINDOW_WIDTH,
                             res.rawUnsigned,
                             defaultWidth);
                } else {
                    LOG_WARN(core::log::cat::Config,
                             "[SkyGuard::WindowConfigLoader] Некорректное поле '{}': "
                             "ожидалось целое число. Применён дефолт ({}).",
                             gk::WINDOW_WIDTH,
                             defaultWidth);
                }
            }
        }

        {
            const unsigned defaultHeight = cfg.height;
            const auto res =
                json_utils::parseUnsignedWithIssue(data, gk::WINDOW_HEIGHT, defaultHeight);

            cfg.height = res.value;

            using Kind = json_utils::UnsignedParseIssue::Kind;
            if (res.issue.kind != Kind::None && res.issue.kind != Kind::MissingKey) {

                if (res.issue.kind == Kind::Negative) {
                    LOG_WARN(core::log::cat::Config,
                             "[SkyGuard::WindowConfigLoader] Некорректное поле '{}': "
                             "значение отрицательное ({}). Применён дефолт ({}).",
                             gk::WINDOW_HEIGHT,
                             res.rawSigned,
                             defaultHeight);
                } else if (res.issue.kind == Kind::OutOfRange) {
                    LOG_WARN(core::log::cat::Config,
                             "[SkyGuard::WindowConfigLoader] Некорректное поле '{}': "
                             "значение вне диапазона ({}). Применён дефолт ({}).",
                             gk::WINDOW_HEIGHT,
                             res.rawUnsigned,
                             defaultHeight);
                } else {
                    LOG_WARN(core::log::cat::Config,
                             "[SkyGuard::WindowConfigLoader] Некорректное поле '{}': "
                             "ожидалось целое число. Применён дефолт ({}).",
                             gk::WINDOW_HEIGHT,
                             defaultHeight);
                }
            }
        }

        cfg.title = json_utils::parseValue<std::string>(data, gk::WINDOW_TITLE, cfg.title);

        return cfg;
    }

} // namespace game::skyguard::config