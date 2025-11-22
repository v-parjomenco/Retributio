#include "pch.h"

#include "game/skyguard/config/loader/config_loader.h"

#include "core/config/config_keys.h"
#include "core/resources/ids/resource_id_utils.h"
#include "core/ui/ids/ui_id_utils.h"
#include "core/utils/file_loader.h"
#include "core/utils/json/json_utils.h"
#include "core/utils/message.h"

namespace {

    using core::utils::FileLoader;

    namespace message = core::utils::message;
    namespace keys = core::config::keys;
    namespace json_utils = core::utils::json;
    namespace rids = core::resources::ids;
    namespace ui_ids = core::ui::ids;

    using Json = json_utils::json;

} // namespace

namespace game::skyguard::config {

    blueprints::PlayerBlueprint ConfigLoader::loadPlayerConfig(const std::string& path) {

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

            return blueprints::PlayerBlueprint{}; // дефолт из PlayerBlueprint+properties
        }

        const std::string& fileContent = *fileContentOpt;

        /**
        * @brief Общий хелпер: парсинг + валидация для "критичного" конфига.
        * В случае любой ошибки:
        *  - пользователь увидит showError,
        *  - приложение завершится через std::exit(EXIT_FAILURE).
        */
        Json data = json_utils::parseAndValidateCritical(
            fileContent, path, "ConfigLoader",
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
                {keys::Player::RESIZE_SCALING, {Json::value_t::string}, false},
                {keys::Player::LOCK_BEHAVIOR, {Json::value_t::string}, false},
                {keys::Player::CONTROLS, {Json::value_t::object}, false},
            });

        // Используем PlayerBlueprint, в котором лежат properties::Sprite/Movement/Anchor/Controls
        blueprints::PlayerBlueprint
            cfg; // создаём со значениями по умолчанию из properties + config.h

        // ------------------------------------------------------------------------------------
        // Текстура: строковый ID ("Player" из player.json) → enum TextureID
        // ------------------------------------------------------------------------------------
        {
            // Читаем строковый идентификатор текстуры, например "Player".
            // В JSON это НЕ путь, а логическое имя, которое должно совпадать
            // с именем в resources.json и с toString(TextureID).
            const std::string defaultTextureIdStr = rids::toString(cfg.sprite.textureId);

            const std::string textureIdStr = json_utils::parseValue<std::string>(
                data, keys::Player::TEXTURE, defaultTextureIdStr);

            if (auto idOpt = rids::textureFromString(textureIdStr)) {
                cfg.sprite.textureId = *idOpt;
            } else {
                message::showError("[ConfigLoader]\nНеизвестный texture ID: " + textureIdStr +
                                   ". Применено значение по умолчанию (" + defaultTextureIdStr +
                                   ").");
                // cfg.sprite.textureId уже содержит дефолт — оставляем его без изменений.
            }
        }

        // Коэффициент масштабирования спрайта игрока
        cfg.sprite.scale =
            json_utils::parseValue<sf::Vector2f>(data, keys::Player::SCALE, cfg.sprite.scale);

        // Параметры движения игрока
        cfg.movement.speed =
            json_utils::parseValue<float>(data, keys::Player::SPEED, cfg.movement.speed);
        cfg.movement.acceleration = json_utils::parseValue<float>(data, keys::Player::ACCELERATION,
                                                                  cfg.movement.acceleration);
        cfg.movement.friction =
            json_utils::parseValue<float>(data, keys::Player::FRICTION, cfg.movement.friction);

        // ----------------------------------------------------------------------------------------
        // Параметры привязки и поведения камеры/экрана
        // ----------------------------------------------------------------------------------------

        // anchor: строка -> AnchorType (enum в AnchorProperties)
        {
            // Если в JSON нет поля "anchor" — используем строковый дефолт из config.h,
            // затем конвертируем в enum через ui::ids::anchorFromString.
            const std::string anchorStr = json_utils::parseValue<std::string>(
                data, keys::Player::ANCHOR, ::config::DEFAULT_ANCHOR);

            cfg.anchor.anchorType = ui_ids::anchorFromString(anchorStr, cfg.anchor.anchorType);
        }

        // Стартовая позиция в мировых координатах
        cfg.anchor.startPosition = json_utils::parseValue<sf::Vector2f>(
            data, keys::Player::START_POSITION, cfg.anchor.startPosition);

        // resize_scaling: строка -> ScalingBehaviorKind
        {
            const std::string scalingStr = json_utils::parseValue<std::string>(
                data, keys::Player::RESIZE_SCALING, ::config::DEFAULT_RESIZE_SCALING);

            cfg.anchor.scalingBehavior =
                ui_ids::scalingFromString(scalingStr, cfg.anchor.scalingBehavior);
        }

        // lock_behavior: строка -> LockBehaviorKind
        {
            const std::string lockStr = json_utils::parseValue<std::string>(
                data, keys::Player::LOCK_BEHAVIOR, ::config::DEFAULT_LOCK_BEHAVIOR);

            cfg.anchor.lockBehavior = ui_ids::lockFromString(lockStr, cfg.anchor.lockBehavior);
        }

        // Обработка блока управляющих клавиш (если он есть)
        // Здесь мы находим конкретные кнопки движения, которые могут быть переопределены в JSON.
        if (data.contains(keys::Player::CONTROLS) && data.at(keys::Player::CONTROLS).is_object()) {
            const auto& c = data.at(keys::Player::CONTROLS);

            // Каждая клавиша может быть переопределена в JSON
            cfg.controls.up = json_utils::parseKey(c, keys::Player::CONTROL_UP, cfg.controls.up);
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

        // Возвращаем полностью собранный PlayerConfig
        return cfg;
    }

} // namespace game::skyguard::config