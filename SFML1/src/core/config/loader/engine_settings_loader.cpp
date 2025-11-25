#include "pch.h"

#include "core/config/loader/engine_settings_loader.h"

#include "core/config/config_keys.h"
#include "core/utils/file_loader.h"
#include "core/utils/json/json_utils.h"
#include "core/utils/message.h"

namespace {

    using core::utils::FileLoader;

    namespace message = core::utils::message;
    namespace json_utils = core::utils::json;
    namespace eng_keys = core::config::keys::EngineSettings;

    using Json = json_utils::json;

} // namespace

namespace core::config {

    EngineSettings loadEngineSettings(const std::string& path) {
        EngineSettings cfg; // стартуем с дефолтов (источник истины №1)

        // ----------------------------------------------------------------------------------------
        // Низкоуровневое чтение файла
        // ----------------------------------------------------------------------------------------
        const auto fileContentOpt = FileLoader::loadTextFile(path);
        if (!fileContentOpt) {
            message::logDebug(
                "[EngineSettings]\nФайл не найден или не читается, используются значения по "
                "умолчанию: " +
                path);
            return cfg;
        }

        const std::string& fileContent = *fileContentOpt;

        // ----------------------------------------------------------------------------------------
        // Парсинг + валидация как НЕ критичного конфига.
        // При любой ошибке: остаёмся на дефолтных EngineSettings.
        // ----------------------------------------------------------------------------------------
        auto dataOpt = json_utils::parseAndValidateNonCritical(
            fileContent, path, "EngineSettings",
            {
                {eng_keys::VSYNC, {Json::value_t::boolean}, false},
                {eng_keys::FRAME_LIMIT,
                 {Json::value_t::number_integer, Json::value_t::number_unsigned},
                 false},
            });

        if (!dataOpt) {
            return cfg;
        }

        const Json& data = *dataOpt;

        // ----------------------------------------------------------------------------------------
        // Заполнение полей из JSON с fallback на EngineSettings по умолчанию.
        // ----------------------------------------------------------------------------------------

        // vsync (bool)
        cfg.vsyncEnabled = json_utils::parseValue<bool>(data, eng_keys::VSYNC, cfg.vsyncEnabled);

        // frameLimit (unsigned)
        cfg.frameLimit =
            json_utils::parseValue<unsigned>(data, eng_keys::FRAME_LIMIT, cfg.frameLimit);

        return cfg;
    }

} // namespace core::config