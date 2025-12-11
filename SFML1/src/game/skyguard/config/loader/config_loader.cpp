#include "pch.h"

#include "game/skyguard/config/loader/config_loader.h"

#include "core/log/log_macros.h"
#include "core/resources/ids/resource_id_utils.h"
#include "core/ui/ids/ui_id_utils.h"
#include "core/utils/file_loader.h"
#include "core/utils/json/json_utils.h"

#include "game/skyguard/config/config_keys.h"

namespace {

    using core::utils::FileLoader;

    namespace keys      = game::skyguard::config::keys;
    namespace json_utils = core::utils::json;
    namespace rids      = core::resources::ids;
    namespace ui_ids    = core::ui::ids;

    using Json = json_utils::json;

} // namespace

namespace game::skyguard::config {

    blueprints::PlayerBlueprint ConfigLoader::loadPlayerConfig(const std::string& path) {

        // ----------------------------------------------------------------------------------------
        // Низкоуровневое чтение файла
        // ----------------------------------------------------------------------------------------
        const auto fileContentOpt = FileLoader::loadTextFile(path);
        if (!fileContentOpt) {
            // Type B config: I/O-ошибка → WARN + дефолтный PlayerBlueprint.
            // FileLoader уже залогировал низкоуровневую I/O-проблему (Engine/DEBUG).
            LOG_WARN(core::log::cat::Config,
                     "[ConfigLoader]\nНе удалось открыть конфигурацию игрока: {}. "
                     "Будут использованы значения по умолчанию.",
                     path);
            return blueprints::PlayerBlueprint{}; // дефолт из PlayerBlueprint + properties
        }
        const std::string& fileContent = *fileContentOpt;

        // ----------------------------------------------------------------------------------------
        // Парсинг + валидация как "критичного" конфига
        // ----------------------------------------------------------------------------------------
        const Json data = json_utils::parseAndValidateCritical(
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

                // Параметры движения игрока также считаем обязательными.
                {keys::Player::SPEED,        {Json::value_t::number_float}},
                {keys::Player::ACCELERATION, {Json::value_t::number_float}},
                {keys::Player::FRICTION,     {Json::value_t::number_float}},

                // Остальные параметры — необязательны, возможны значения по умолчанию.
                {keys::Player::ANCHOR,         {Json::value_t::string},                 false},
                {keys::Player::START_POSITION, {Json::value_t::object, Json::value_t::array}, false},
                {keys::Player::RESIZE_SCALING, {Json::value_t::string},                 false},
                {keys::Player::LOCK_BEHAVIOR,  {Json::value_t::string},                 false},
                {keys::Player::CONTROLS,       {Json::value_t::object},                 false},
            });

        // Используем PlayerBlueprint, в котором лежат properties::Sprite/Movement/Anchor/Controls
        blueprints::PlayerBlueprint
            cfg{}; // создаём со значениями по умолчанию из properties (enum-дефолты)

        // ----------------------------------------------------------------------------------------
        // Текстура: строковый ID ("Player" из player.json) → enum TextureID
        // ----------------------------------------------------------------------------------------
        {
            // Enum-дефолт уже лежит в cfg.sprite.textureId, здесь его не трогаем.
            //
            // JsonValidator + parseAndValidateCritical гарантируют, что ключ TEXTURE:
            //  - присутствует;
            //  - имеет тип string.
            //
            // Тем не менее, parseEnum остаётся "мягким" helper'ом:
            // если правила валидации когда-нибудь ослабят, мы просто оставим дефолт
            // и не упадём в runtime (логика фатальности лежит в JsonValidator + panic-слое).
            cfg.sprite.textureId = json_utils::parseEnum(
                data, keys::Player::TEXTURE,
                cfg.sprite.textureId, // defaultValue — уже корректный enum-дефолт
                rids::textureFromString, [&](std::string_view badId) {
                    const std::string_view defaultTextureIdStr =
                        rids::toString(cfg.sprite.textureId);

                    LOG_WARN(core::log::cat::Config,
                             "[ConfigLoader]\nНеизвестный texture ID: {}. Применено значение по "
                             "умолчанию ({}).",
                             badId, defaultTextureIdStr);
                    // cfg.sprite.textureId остаётся равен defaultValue.
                });
        }

        // ----------------------------------------------------------------------------------------
        // Масштаб и движение
        // ----------------------------------------------------------------------------------------
        cfg.sprite.scale =
            json_utils::parseValue<sf::Vector2f>(data, keys::Player::SCALE, cfg.sprite.scale);

        cfg.movement.maxSpeed =
            json_utils::parseValue<float>(data, keys::Player::SPEED, cfg.movement.maxSpeed);
        cfg.movement.acceleration = json_utils::parseValue<float>(
            data, keys::Player::ACCELERATION, cfg.movement.acceleration);
        cfg.movement.friction =
            json_utils::parseValue<float>(data, keys::Player::FRICTION, cfg.movement.friction);

        // ----------------------------------------------------------------------------------------
        // Параметры привязки и поведения камеры/экрана
        // ----------------------------------------------------------------------------------------

        // anchor: строка -> AnchorType (enum в AnchorProperties)
        {
            // Если в JSON нет поля "anchor" — остаётся enum-дефолт из AnchorProperties.
            if (auto anchorStr =
                    json_utils::getOptional<std::string_view>(data, keys::Player::ANCHOR)) {
                cfg.anchor.anchorType =
                    ui_ids::anchorFromString(*anchorStr, cfg.anchor.anchorType);
            }
        }

        // Стартовая позиция в мировых координатах
        cfg.anchor.startPosition = json_utils::parseValue<sf::Vector2f>(
            data, keys::Player::START_POSITION, cfg.anchor.startPosition);

        // resize_scaling: строка -> ScalingBehaviorKind
        {
            // Если в JSON нет поля "resize_scaling" — остаётся enum-дефолт.
            if (auto scalingStr =
                    json_utils::getOptional<std::string_view>(data, keys::Player::RESIZE_SCALING)) {
                cfg.anchor.scalingBehavior =
                    ui_ids::scalingFromString(*scalingStr, cfg.anchor.scalingBehavior);
            }
        }

        // lock_behavior: строка -> LockBehaviorKind
        {
            // Если в JSON нет поля "lock_behavior" — остаётся enum-дефолт.
            if (auto lockStr =
                    json_utils::getOptional<std::string_view>(data, keys::Player::LOCK_BEHAVIOR)) {
                cfg.anchor.lockBehavior =
                    ui_ids::lockFromString(*lockStr, cfg.anchor.lockBehavior);
            }
        }

        // ----------------------------------------------------------------------------------------
        // Управляющие клавиши
        // ----------------------------------------------------------------------------------------
        const auto controlsIt = data.find(keys::Player::CONTROLS);
        if (controlsIt != data.end() && controlsIt->is_object()) {
            const auto& c = *controlsIt;

            cfg.controls.up =
                json_utils::parseKey(c, keys::Player::CONTROL_UP, cfg.controls.up);
            cfg.controls.down =
                json_utils::parseKey(c, keys::Player::CONTROL_DOWN, cfg.controls.down);
            cfg.controls.left =
                json_utils::parseKey(c, keys::Player::CONTROL_LEFT, cfg.controls.left);
            cfg.controls.right =
                json_utils::parseKey(c, keys::Player::CONTROL_RIGHT, cfg.controls.right);
        } else {
            // Нет блока controls — не фатально, просто логируем и оставляем дефолт.
            LOG_WARN(core::log::cat::Config,
                     "[ConfigLoader]\nБлок controls отсутствует или имеет неверный тип. "
                     "Применены значения по умолчанию.");
        }

        // Возвращаем полностью собранную конфигурацию игрока
        return cfg;
    }

} // namespace game::skyguard::config