#include "pch.h"

#include "core/config/config_loader.h"

#include "core/config/config_keys.h"
#include "core/resources/ids/id_to_string.h"
#include "core/utils/file_loader.h"
#include "core/utils/json/json_utils.h"
#include "core/utils/message.h"

namespace {

    using core::utils::FileLoader;

    namespace message = core::utils::message;
    namespace keys = core::config::keys;
    namespace json_utils = core::utils::json;
    namespace rids = core::resources::ids;

    using Json          = json_utils::json;

} // namespace

namespace core::config {

    PlayerConfig ConfigLoader::loadPlayerConfig(const std::string& path) {

        /**
        * @brief Низкоуровневое чтение файла через FileLoader.
        * На этом шаге нас интересуют только два факта:
        *  - получилось ли прочитать файл целиком;
        *  - если нет —> логируем это в debug и показываем пользователю окно об ошибке.
        */
        const auto fileContentOpt = FileLoader::loadTextFile(path);
        if (!fileContentOpt) {
            // FileLoader уже залогировал низкоуровневую проблему с файлом,
            // здесь даём пользователю понятный popup и возвращаем дефолтные значения.
            message::showError("[ConfigLoader]\nНе удалось открыть конфигурацию игрока: " + path +
                               ".\nБудут использованы значения по умолчанию.");

            return PlayerConfig{}; // безопасный дефолт из config.h + базовых ID
        }

        const std::string& fileContent = *fileContentOpt;

        /**
        * @brief Общий хелпер: парсинг + валидация для "критичного" конфига.
        * В случае любой ошибки:
        *  - пользователь увидит showError,
        *  - приложение завершится через std::exit(EXIT_FAILURE).
        */
        Json data = json_utils::parseAndValidateCritical(
            fileContent,
            path,
            "ConfigLoader",
            {
                // Обязательный: строковый ID текстуры (например, "Player")
                {keys::Player::TEXTURE, {Json::value_t::string}},

                // Масштаб может быть:
                //  - числом       (равный по X и Y),
                //  - объектом     { "x": 1.0, "y": 2.0 },
                //  - массивом     [1.0, 2.0]
                {keys::Player::SCALE,
                 {Json::value_t::number_float, Json::value_t::object, Json::value_t::array}},

                // Остальные параметры — необязательны, возможны значения по умолчанию.
                {keys::Player::SPEED, {Json::value_t::number_float}, false},
                {keys::Player::ACCELERATION, {Json::value_t::number_float}, false},
                {keys::Player::FRICTION, {Json::value_t::number_float}, false},
                {keys::Player::ANCHOR, {Json::value_t::string}, false},
                {keys::Player::START_POSITION,
                 {Json::value_t::object, Json::value_t::array},
                 false},
                {keys::Player::SCALING, {Json::value_t::string}, false},
                {keys::Player::LOCK_BEHAVIOR, {Json::value_t::string}, false},
                {keys::Player::CONTROLS, {Json::value_t::object}, false},
            });

        // Заполнение PlayerConfig на основе JSON-данных, считанных с помощью json_utils

        PlayerConfig cfg; // создаём со значениями по умолчанию из config.h и enum'ов

        //  - если ключ есть и валиден → используем значение из JSON,
        //  - если ключа нет → оставляем значение по умолчанию из структуры PlayerConfig.

        // ------------------------------------------------------------------------------------
        // Текстура: строковый ID ("Player" из player.json) → enum TextureID
        // ------------------------------------------------------------------------------------
        {
            // Читаем строковый идентификатор текстуры, например "Player".
            // В JSON это НЕ путь, а логическое имя, которое должно совпадать
            // с именем в resources.json и с toString(TextureID).
            const std::string defaultTextureIdStr = rids::toString(cfg.textureId);
            const std::string textureIdStr = json_utils::parseValue<std::string>(
                data, keys::Player::TEXTURE, defaultTextureIdStr);

            if (auto idOpt = rids::textureFromString(textureIdStr)) {
                cfg.textureId = *idOpt;
            } else {
                message::showError("[ConfigLoader]\nНеизвестный texture ID: " + textureIdStr +
                                   ". Применено значение по умолчанию (" + defaultTextureIdStr +
                                   ").");
                // cfg.textureId уже содержит дефолт из config.h — оставляем его без изменений.
            }
        }

        // Коэффициент масштабирования спрайта игрока
        cfg.scale =
            json_utils::parseValue<sf::Vector2f>(data, keys::Player::SCALE, cfg.scale);

        // Параметры движения игрока
        cfg.speed =
            json_utils::parseValue<float>(data, keys::Player::SPEED, cfg.speed);
        cfg.acceleration =
            json_utils::parseValue<float>(data, keys::Player::ACCELERATION, cfg.acceleration);
        cfg.friction = 
            json_utils::parseValue<float>(data, keys::Player::FRICTION, cfg.friction);

        // Параметры привязки и поведения камеры/экрана
        cfg.anchor =
            json_utils::parseValue<std::string>(data, keys::Player::ANCHOR, cfg.anchor);
        cfg.startPosition = json_utils::parseValue<sf::Vector2f>(
            data, keys::Player::START_POSITION, cfg.startPosition);
        cfg.scaling =
            json_utils::parseValue<std::string>(data, keys::Player::SCALING, cfg.scaling);
        cfg.lockBehavior =
            json_utils::parseValue<std::string>(
            data, keys::Player::LOCK_BEHAVIOR, cfg.lockBehavior);

        // Обработка блока управляющих клавиш (если он есть)
        // Здесь мы находим конкретные кнопки движения, которые могут быть переопределены в JSON.
        if (data.contains(keys::Player::CONTROLS) && data.at(keys::Player::CONTROLS).is_object()) {
            const auto& c = data.at(keys::Player::CONTROLS);

            // Каждая клавиша может быть переопределена в JSON
            cfg.controls.up = 
                json_utils::parseKey(c, keys::Player::CONTROL_UP, cfg.controls.up);
            cfg.controls.down =
                json_utils::parseKey(c, keys::Player::CONTROL_DOWN, cfg.controls.down);
            cfg.controls.left =
                json_utils::parseKey(c, keys::Player::CONTROL_LEFT, cfg.controls.left);
            cfg.controls.right =
                json_utils::parseKey(c, keys::Player::CONTROL_RIGHT, cfg.controls.right);
        } else {
            // Нет блока controls — не фатально, просто сообщаем и оставляем дефолт.
            message::showError("[ConfigLoader]\nБлок controls отсутствует или имеет неверный тип. "
                               "Применены значения по умолчанию.");
        }

        // scaling может быть только "Uniform" или "None"
        if (cfg.scaling != "Uniform" && cfg.scaling != "None") {
            message::showError(
                std::string("[ConfigLoader]\nНеизвестное значение scaling: ") + cfg.scaling +
                ". Применено значение по умолчанию (None).");
            cfg.scaling = "None";
        }

        // lockBehavior может быть только "ScreenLock" или "WorldLock"
        if (cfg.lockBehavior != "ScreenLock" && cfg.lockBehavior != "WorldLock") {
            message::showError(
                std::string("[ConfigLoader]\nНеизвестное значение lock_behavior: ") +
                cfg.lockBehavior + ". Применено значение по умолчанию (WorldLock).");
            cfg.lockBehavior = "WorldLock";
        }

        // anchor должен конвертироваться в валидный AnchorType
        if (core::ui::anchors::fromString(cfg.anchor) == core::ui::AnchorType::None &&
            cfg.anchor != "None") {
            message::showError(
                std::string("[ConfigLoader]\nНеизвестное значение anchor: ") + cfg.anchor +
                ". Применено значение по умолчанию (None).");
            cfg.anchor = "None";
        }

        // Возвращаем полностью собранный PlayerConfig
        return cfg;
    }

} // namespace core::config