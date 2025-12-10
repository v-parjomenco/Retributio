#include "pch.h"

#include "core/config/loader/engine_settings_loader.h"

#include "core/config/config_keys.h"
#include "core/log/log_macros.h"
#include "core/utils/file_loader.h"
#include "core/utils/json/json_utils.h"

namespace {

    using core::utils::FileLoader;

    namespace json_utils = core::utils::json;
    namespace eng_keys = core::config::keys::EngineSettings;

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
            // Type B config: I/O-ошибка → WARN + безопасные дефолты (см. общую политику логирования).
            LOG_WARN(core::log::cat::Config,
                      "[EngineSettings]\nФайл настроек движка не найден или не читается: {}. "
                      "Используются значения по умолчанию (vsyncEnabled = {}, frameLimit = {}).",
                      path, cfg.vsyncEnabled, cfg.frameLimit);
            return cfg;
        }

        // ----------------------------------------------------------------------------------------
        // Парсинг + валидация как НЕ критичного конфига.
        // При ошибке parseAndValidateNonCritical вернёт std::nullopt и запишет подробности в лог.        
        // ----------------------------------------------------------------------------------------
        const auto dataOpt = json_utils::parseAndValidateNonCritical(
            *fileContentOpt, path, "EngineSettings",
            {
                {eng_keys::VSYNC, {Json::value_t::boolean}, false},
                {eng_keys::FRAME_LIMIT,
                 {Json::value_t::number_integer, Json::value_t::number_unsigned},
                 false},
            }
        );
        if (!dataOpt) {
            return cfg;
        }

        const Json& data = *dataOpt;

        // ----------------------------------------------------------------------------------------
        // Заполнение полей из JSON с fallback на EngineSettings по умолчанию.
        // ----------------------------------------------------------------------------------------

        cfg.vsyncEnabled = json_utils::parseValue<bool>(data, eng_keys::VSYNC, cfg.vsyncEnabled);
        cfg.frameLimit =
            json_utils::parseValue<unsigned>(data, eng_keys::FRAME_LIMIT, cfg.frameLimit);

        return cfg;
    }

} // namespace core::config