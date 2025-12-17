#include "pch.h"

#include "game/skyguard/config/loader/config_loader.h"

#include <array>

#include <SFML/Window/Keyboard.hpp>

#include "core/log/log_macros.h"
#include "core/resources/ids/resource_id_utils.h"
#include "core/ui/ids/ui_id_utils.h"
#include "core/utils/file_loader.h"
#include "core/utils/json/json_accessors.h"
#include "core/utils/json/json_document.h"
#include "core/utils/json/json_parsers.h"

#include "game/skyguard/config/config_keys.h"

namespace {

    using core::utils::FileLoader;

    namespace keys       = game::skyguard::config::keys;
    namespace json_utils = core::utils::json;
    namespace rids       = core::resources::ids;
    namespace ui_ids     = core::ui::ids;

    using Json = json_utils::json;

    struct ControlsLayoutIssue {
        enum class Kind {
            None,
            Unknown,
            Duplicate
        };

        Kind kind{Kind::None};
        sf::Keyboard::Key key{sf::Keyboard::Key::Unknown};
    };

    [[nodiscard]] ControlsLayoutIssue validateControlsLayout(
        sf::Keyboard::Key up,
        sf::Keyboard::Key down,
        sf::Keyboard::Key left,
        sf::Keyboard::Key right) noexcept
    {
        const std::array<sf::Keyboard::Key, 4> keysArr{up, down, left, right};

        for (const auto k : keysArr) {
            if (k == sf::Keyboard::Key::Unknown) {
                return {ControlsLayoutIssue::Kind::Unknown, k};
            }
        }

        for (std::size_t i = 0; i < keysArr.size(); ++i) {
            for (std::size_t j = i + 1; j < keysArr.size(); ++j) {
                if (keysArr[i] == keysArr[j]) {
                    return {ControlsLayoutIssue::Kind::Duplicate, keysArr[i]};
                }
            }
        }

        return {};
    }

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
                     "[ConfigLoader] Не удалось открыть конфигурацию игрока: {}. "
                     "Будут использованы значения по умолчанию.",
                     path);
            return blueprints::PlayerBlueprint{};
        }
        const std::string& fileContent = *fileContentOpt;

        // ----------------------------------------------------------------------------------------
        // Парсинг + структурная валидация "критичного" конфига
        // ----------------------------------------------------------------------------------------
        const Json data = json_utils::parseAndValidateCritical(
            fileContent,
            path,
            "ConfigLoader",
            {
                {keys::Player::TEXTURE, {Json::value_t::string}},

                // Масштаб может быть:
                //  - числом       (равный по X и Y),
                //  - объектом     { "x": 1.0, "y": 2.0 },
                //  - массивом     [1.0, 2.0]
                {keys::Player::SCALE,
                 {
                     Json::value_t::number_float,
                     Json::value_t::number_integer,
                     Json::value_t::number_unsigned,
                     Json::value_t::object,
                     Json::value_t::array
                 }},

                {keys::Player::SPEED,
                 {Json::value_t::number_float, Json::value_t::number_integer,
                  Json::value_t::number_unsigned}},
                {keys::Player::ACCELERATION,
                 {Json::value_t::number_float, Json::value_t::number_integer,
                  Json::value_t::number_unsigned}},
                {keys::Player::FRICTION,
                 {Json::value_t::number_float, Json::value_t::number_integer,
                  Json::value_t::number_unsigned}},

                {keys::Player::ANCHOR,         {Json::value_t::string}, false},
                {keys::Player::START_POSITION, {Json::value_t::object, Json::value_t::array}, false},
                {keys::Player::RESIZE_SCALING, {Json::value_t::string}, false},
                {keys::Player::LOCK_BEHAVIOR,  {Json::value_t::string}, false},
                {keys::Player::CONTROLS,       {Json::value_t::object}, false},
            });

        // Дефолты — из properties внутри PlayerBlueprint.
        blueprints::PlayerBlueprint cfg{};

        // ----------------------------------------------------------------------------------------
        // Текстура: строковый ID → enum TextureID
        // ----------------------------------------------------------------------------------------

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
            data,
            keys::Player::TEXTURE,
            cfg.sprite.textureId,
            rids::textureFromString,
            [&](std::string_view badId) {
                const std::string_view defaultTextureIdStr = rids::toString(cfg.sprite.textureId);

                LOG_WARN(core::log::cat::Config,
                         "[ConfigLoader] Неизвестный texture ID: {}. "
                         "Применено значение по умолчанию ({}).",
                         badId,
                         defaultTextureIdStr);
            });

        // ----------------------------------------------------------------------------------------
        // Масштаб и движение
        // ----------------------------------------------------------------------------------------

        {
            const auto scaleRes =
                json_utils::parseVec2fWithIssue(data, keys::Player::SCALE, cfg.sprite.scale);

            cfg.sprite.scale = scaleRes.value;

            using Kind = json_utils::Vec2ParseIssue::Kind;
            if (scaleRes.issue.kind != Kind::None && scaleRes.issue.kind != Kind::MissingKey) {

                LOG_WARN(core::log::cat::Config,
                         "[ConfigLoader] Некорректное поле '{}': {}. "
                         "Применён дефолт scale=({:.3f}, {:.3f}).",
                         keys::Player::SCALE, json_utils::describe(scaleRes.issue),
                         cfg.sprite.scale.x, cfg.sprite.scale.y);
            }
        }

        cfg.movement.maxSpeed =
            json_utils::parseValue<float>(data, keys::Player::SPEED, cfg.movement.maxSpeed);
        cfg.movement.acceleration = json_utils::parseValue<float>(
            data, keys::Player::ACCELERATION, cfg.movement.acceleration);
        cfg.movement.friction =
            json_utils::parseValue<float>(data, keys::Player::FRICTION, cfg.movement.friction);

        // ----------------------------------------------------------------------------------------
        // Anchor / resize поведение
        // ----------------------------------------------------------------------------------------

        // anchor: строка -> AnchorType (enum в AnchorProperties)
        // Если в JSON нет поля "anchor" — остаётся enum-дефолт из AnchorProperties.
        if (auto anchorStr =
                json_utils::getOptional<std::string_view>(data, keys::Player::ANCHOR)) {
            cfg.anchor.anchorType =
                ui_ids::anchorFromString(*anchorStr, cfg.anchor.anchorType);
        }

        // Стартовая позиция в мировых координатах
        cfg.anchor.startPosition = json_utils::parseValue<sf::Vector2f>(
            data, keys::Player::START_POSITION, cfg.anchor.startPosition);

        // resize_scaling: строка -> ScalingBehaviorKind
        // Если в JSON нет поля "resize_scaling" — остаётся enum-дефолт.
        if (auto scalingStr =
                json_utils::getOptional<std::string_view>(data, keys::Player::RESIZE_SCALING)) {
            cfg.anchor.scalingBehavior =
                ui_ids::scalingFromString(*scalingStr, cfg.anchor.scalingBehavior);
        }

        // lock_behavior: строка -> LockBehaviorKind
        // Если в JSON нет поля "lock_behavior" — остаётся enum-дефолт.
        if (auto lockStr =
                json_utils::getOptional<std::string_view>(data, keys::Player::LOCK_BEHAVIOR)) {
            cfg.anchor.lockBehavior =
                ui_ids::lockFromString(*lockStr, cfg.anchor.lockBehavior);
        }

        // ----------------------------------------------------------------------------------------
        // Управляющие клавиши (семантическая валидация: Unknown/дубли)
        // ----------------------------------------------------------------------------------------
        const auto controlsIt = data.find(keys::Player::CONTROLS);
        if (controlsIt != data.end()) {
            const auto& c = *controlsIt;

            auto parsed = cfg.controls;
            // Диагностика уровня loader: json_utils ничего не логирует.
            auto applyKey = [&](std::string_view controlKey, sf::Keyboard::Key& dst) {
                const auto res = json_utils::parseKeyWithIssue(c, controlKey, dst);
                dst = res.value;

                using Kind = json_utils::KeyParseIssue::Kind;
                if (res.issue.kind == Kind::WrongType) {

                    LOG_WARN(core::log::cat::Config,
                             "[ConfigLoader] Некорректное поле controls '{}': ожидалась строка. "
                             "Применён дефолт.",
                             controlKey);
                } else if (res.issue.kind == Kind::UnknownName) {

                    LOG_WARN(core::log::cat::Config,
                             "[ConfigLoader] Некорректное поле controls '{}': неизвестная "
                             "клавиша '{}'. Применён дефолт.",
                             controlKey, res.rawName);
                }
            };

            applyKey(keys::Player::CONTROL_UP, parsed.up);
            applyKey(keys::Player::CONTROL_DOWN, parsed.down);
            applyKey(keys::Player::CONTROL_LEFT, parsed.left);
            applyKey(keys::Player::CONTROL_RIGHT, parsed.right);

            const auto issue =
                validateControlsLayout(parsed.up, parsed.down, parsed.left, parsed.right);

            if (issue.kind != ControlsLayoutIssue::Kind::None) {
                const char* reason = "неизвестная причина";

                switch (issue.kind) {
                case ControlsLayoutIssue::Kind::Unknown:
                    reason = "обнаружен Unknown";
                    break;
                case ControlsLayoutIssue::Kind::Duplicate:
                    reason = "обнаружены дубли";
                    break;
                case ControlsLayoutIssue::Kind::None:
                default:
                    break;
                }

                LOG_WARN(core::log::cat::Config,
                         "[ConfigLoader] Невалидная раскладка controls ({}; key={}). "
                         "Применены значения по умолчанию. "
                         "(up={}, down={}, left={}, right={})",
                         reason,
                         static_cast<int>(issue.key),
                         static_cast<int>(parsed.up),
                         static_cast<int>(parsed.down),
                         static_cast<int>(parsed.left),
                         static_cast<int>(parsed.right));
            } else {
                cfg.controls = parsed;
            }
        }

        return cfg;
    }

} // namespace game::skyguard::config