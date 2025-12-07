#include "pch.h"

#include "game/skyguard/config/loader/window_config_loader.h"

#include "core/log/log_macros.h"
#include "core/utils/file_loader.h"
#include "core/utils/json/json_utils.h"

#include "game/skyguard/config/config_keys.h"

namespace {

    using core::utils::FileLoader;

    namespace json_utils = core::utils::json;
    namespace gk = game::skyguard::config::keys::Game;

    using Json = json_utils::json;

} // namespace

namespace game::skyguard::config {

    WindowConfig loadWindowConfig(const std::string& path) {
        WindowConfig cfg; // дефолт = источник истины №1

        // ----------------------------------------------------------------------------------------
        // Низкоуровневое чтение файла
        // ----------------------------------------------------------------------------------------
        const auto fileContentOpt = FileLoader::loadTextFile(path);
        if (!fileContentOpt) {
            LOG_DEBUG(core::log::cat::Engine,
                      "[SkyGuard::WindowConfig]\nФайл не найден или не читается, используются "
                      "значения по умолчанию: ", path);
            return cfg;
        }

        const std::string& fileContent = *fileContentOpt;

        // ----------------------------------------------------------------------------------------
        // Парсинг + валидация как НЕ критичного конфига.
        // ----------------------------------------------------------------------------------------
        auto dataOpt = json_utils::parseAndValidateNonCritical(
            fileContent, path, "SkyGuard::WindowConfig",
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
            return cfg;
        }

        const Json& data = *dataOpt;

        // ----------------------------------------------------------------------------------------
        // Заполнение WindowConfig из JSON (fallback на значения по умолчанию)
        // ----------------------------------------------------------------------------------------
        cfg.width = json_utils::parseValue<unsigned>(data, gk::WINDOW_WIDTH, cfg.width);
        cfg.height = json_utils::parseValue<unsigned>(data, gk::WINDOW_HEIGHT, cfg.height);

        cfg.title = json_utils::parseValue<std::string>(data, gk::WINDOW_TITLE, cfg.title);

        return cfg;
    }

} // namespace game::skyguard::config