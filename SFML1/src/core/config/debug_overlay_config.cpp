#include "pch.h"

#include "core/config/debug_overlay_config.h"

#include "core/config/config_keys.h"
#include "core/utils/file_loader.h"
#include "core/utils/json/json_utils.h"
#include "core/utils/message.h"

namespace {

    using core::utils::FileLoader;

    namespace message = core::utils::message;
    namespace keys = core::config::keys;
    namespace json_utils = core::utils::json;

    using Json          = json_utils::json;

} // namespace

namespace core::config {

    DebugOverlayConfig loadDebugOverlayConfig(const std::string& path) {
        DebugOverlayConfig cfg; // стартуем с дефолтных значений

        // Низкоуровневое чтение файла через FileLoader.
        // Debug overlay — вспомогательный модуль, поэтому при ошибках файла
        // мы не останавливаем игру, а просто работаем с дефолтными настройками.
        const auto fileContentOpt = FileLoader::loadTextFile(path);
        if (!fileContentOpt) {
            // FileLoader уже залогировал низкоуровневую проблему с файлом,
            // здесь мы добавляем контекст более высокого уровня.
            message::logDebug(
                "[DebugOverlayConfig]\nБудет использована конфигурация по умолчанию: " + path);
            return cfg;
        }

        const std::string& fileContent = *fileContentOpt;

        // Общий helper: парсинг + валидация для НЕ критичного конфига.
        // При любой ошибке:
        //  - в лог пишется debug-сообщение,
        //  - возвращаем std::nullopt.
        auto dataOpt = json_utils::parseAndValidateNonCritical(
            fileContent,
            path,
            "DebugOverlayConfig",
            {
                {keys::DebugOverlay::ENABLED,
                 {Json::value_t::boolean},
                 false},
                {keys::DebugOverlay::POSITION,
                 {Json::value_t::array,
                  Json::value_t::object,
                  Json::value_t::number_float},
                 false},
                {keys::DebugOverlay::CHARACTER_SIZE,
                 {Json::value_t::number_integer,
                  Json::value_t::number_unsigned},
                 false},
                {keys::DebugOverlay::COLOR,
                 {Json::value_t::string,
                  Json::value_t::object},
                 false},
            });

        // Если парсинг/валидация не удалась — остаёмся на дефолтных настройках.
        if (!dataOpt) {
            return cfg;
        }

        const Json& data = *dataOpt;

        // Заполнение полей на основе JSON-данных, считанных с помощью json_utils
        //  - если ключ есть и валиден → используем значение из JSON,
        //  - если ключа нет → оставляем значение по умолчанию из структуры DebugOverlayConfig.

        // enabled (bool, если значение с JSON-файла не получено —> остаётся по умолчанию "true")
        cfg.enabled =
            json_utils::parseValue(data, keys::DebugOverlay::ENABLED, cfg.enabled);

        // position (Vector2f с поддержкой number/array/object)
        cfg.position =
            json_utils::parseValue<sf::Vector2f>(data, keys::DebugOverlay::POSITION, cfg.position);


        // characterSize (unsigned int, если значения нет —> остаётся значение по умолчанию)
        cfg.characterSize =
            json_utils::parseValue(data, keys::DebugOverlay::CHARACTER_SIZE, cfg.characterSize);

        // color (строка "#RRGGBB"/"#RRGGBBAA" или объект {r,g,b,a})
        cfg.color =
            json_utils::parseColor(data, keys::DebugOverlay::COLOR, cfg.color);

        return cfg;
    }

} // namespace core::config