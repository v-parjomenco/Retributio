#include "pch.h"

#include "core/config/loader/engine_settings_loader.h"

#include "core/config/config_keys.h"
#include "core/log/log_macros.h"
#include "core/utils/file_loader.h"
#include "core/utils/json/json_document.h"
#include "core/utils/json/json_parsers.h"

namespace {

    using core::utils::FileLoader;

    namespace json_utils = core::utils::json;
    namespace eng_keys   = core::config::keys::EngineSettings;

    using Json = json_utils::json;

} // namespace

namespace core::config {

    EngineSettings loadEngineSettings(const std::string& path) {
        // Стартуем с безопасных дефолтов (источник истины №1 для значений по умолчанию).
        EngineSettings cfg{};

        // ----------------------------------------------------------------------------------------
        // Низкоуровневое чтение файла.
        // Ошибка здесь НЕ фатальная: движок может работать с дефолтами.
        // ----------------------------------------------------------------------------------------
        const auto fileContentOpt = FileLoader::loadTextFile(path);
        if (!fileContentOpt) {
            // Type B config: I/O-ошибка → WARN + безопасные дефолты.
            LOG_WARN(core::log::cat::Config,
                     "[EngineSettingsLoader] Файл настроек движка не найден "
                     "или не читается: '{}'. "
                     "Используются значения по умолчанию (vsyncEnabled={}, frameLimit={}).",
                     path,
                     cfg.vsyncEnabled,
                     cfg.frameLimit);
            return cfg;
        }

        // ----------------------------------------------------------------------------------------
        // Парсинг + валидация как НЕ критичного конфига.
        // parseAndValidateNonCritical пишет детали в DEBUG и возвращает nullopt.
        // ----------------------------------------------------------------------------------------
        const auto dataOpt = json_utils::parseAndValidateNonCritical(
            *fileContentOpt,
            path,
            "EngineSettingsLoader",
            {},
            json_utils::kConfigParseOnlyOptions);

        if (!dataOpt) {
            // Type B: JSON/валидация упали → один заметный WARN (Release-видимый) + дефолты.
            LOG_WARN(core::log::cat::Config,
                     "[EngineSettingsLoader] Не удалось разобрать JSON в '{}'. "
                     "Используются значения по умолчанию (vsyncEnabled={}, frameLimit={}). "
                     "Подробности — в логах уровня DEBUG (если включены).",
                     path,
                     cfg.vsyncEnabled,
                     cfg.frameLimit);
            return cfg;
        }

        const Json& data = *dataOpt;

        // ----------------------------------------------------------------------------------------
        // Заполнение полей из JSON с fallback на EngineSettings по умолчанию.
        // ----------------------------------------------------------------------------------------

        const bool hasVsync =
            (data.find(eng_keys::VSYNC) != data.end());
        const bool hasFrameLimit =
            (data.find(eng_keys::FRAME_LIMIT) != data.end());

        if (!hasVsync && !hasFrameLimit) {
            LOG_WARN(core::log::cat::Config,
                     "[EngineSettingsLoader] Конфиг '{}' прочитан, но не содержит "
                     "ни одного известного ключа (vsyncEnabled/frameLimit). "
                     "Используются значения по умолчанию.",
                     path);
            return cfg;
        }

        // vsyncEnabled (bool)
        {
            const auto it = data.find(eng_keys::VSYNC);
            if (it != data.end()) {
                if (it->is_boolean()) {
                    cfg.vsyncEnabled = it->get<bool>();
                } else {
                    LOG_WARN(core::log::cat::Config,
                             "[EngineSettingsLoader] Некорректное поле '{}': "
                             "ожидался boolean. Применён дефолт ({}).",
                             eng_keys::VSYNC,
                             cfg.vsyncEnabled);
                }
            }
        }

        // frameLimit: ловим отрицательные и out-of-range значения,
        // даже если тип формально number_integer.
        {
            const unsigned defaultLimit = cfg.frameLimit;

            const auto res =
                json_utils::parseUnsignedWithIssue(data, eng_keys::FRAME_LIMIT, defaultLimit);

            cfg.frameLimit = res.value;

            using Kind = json_utils::UnsignedParseIssue::Kind;
            if (res.issue.kind != Kind::None && res.issue.kind != Kind::MissingKey) {

                if (res.issue.kind == Kind::Negative) {
                    LOG_WARN(core::log::cat::Config,
                             "[EngineSettingsLoader] Некорректное поле '{}': "
                             "значение отрицательное ({}). Применён дефолт ({}).",
                             eng_keys::FRAME_LIMIT,
                             res.rawSigned,
                             defaultLimit);
                } else if (res.issue.kind == Kind::OutOfRange) {
                    LOG_WARN(core::log::cat::Config,
                             "[EngineSettingsLoader] Некорректное поле '{}': "
                             "значение вне диапазона ({}). Применён дефолт ({}).",
                             eng_keys::FRAME_LIMIT,
                             res.rawUnsigned,
                             defaultLimit);
                } else {
                    // WrongType
                    LOG_WARN(core::log::cat::Config,
                             "[EngineSettingsLoader] Некорректное поле '{}': "
                             "ожидалось целое число. Применён дефолт ({}).",
                             eng_keys::FRAME_LIMIT,
                             defaultLimit);
                }
            }
        }

        return cfg;
    }

} // namespace core::config