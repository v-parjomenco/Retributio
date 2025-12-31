#include "pch.h"

#include "game/skyguard/config/loader/config_loader.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

#include <SFML/Window/Keyboard.hpp>

#include "core/log/log_macros.h"
#include "core/resources/ids/resource_id_utils.h"
#include "core/ui/ids/ui_id_utils.h"
#include "core/utils/file_loader.h"
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
        sf::Keyboard::Key thrustForward,
        sf::Keyboard::Key thrustBackward,
        sf::Keyboard::Key turnLeft,
        sf::Keyboard::Key turnRight) noexcept
    {
        const std::array<sf::Keyboard::Key, 4> keysArr{
            thrustForward, thrustBackward, turnLeft, turnRight
        };

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

    template <typename Enum, typename TryParseFn>
    void applyOptionalEnumWithWarn(const Json& data,
                                   std::string_view key,
                                   Enum& dst,
                                   TryParseFn tryParse,
                                   std::string_view path)
    {
        const auto res = json_utils::parseEnumWithIssue(data, key, dst, tryParse);

        using Kind = json_utils::EnumParseIssue::Kind;
        switch (res.issue.kind) {
        case Kind::None:
            dst = res.value;
            return;

        case Kind::MissingKey:
            return;

        case Kind::WrongType:
            LOG_WARN(core::log::cat::Config,
                     "[ConfigLoader] Некорректное поле '{}': {}. "
                     "Применено значение по умолчанию ({}). file={}",
                     key,
                     json_utils::describe(res.issue),
                     ui_ids::toString(dst),
                     path);
            return;

        case Kind::UnknownValue:
            LOG_WARN(core::log::cat::Config,
                     "[ConfigLoader] Некорректное поле '{}': {} ('{}'). "
                     "Применено значение по умолчанию ({}). file={}",
                     key,
                     json_utils::describe(res.issue),
                     res.issue.raw,
                     ui_ids::toString(dst),
                     path);
            return;

        default:
            LOG_WARN(core::log::cat::Config,
                     "[ConfigLoader] Некорректное поле '{}': неизвестная ошибка. "
                     "Применено значение по умолчанию ({}). file={}",
                     key,
                     ui_ids::toString(dst),
                     path);
            return;
        }
    }

} // namespace

namespace game::skyguard::config {

    blueprints::PlayerBlueprint ConfigLoader::loadPlayerConfig(const std::string& path) {

        // ----------------------------------------------------------------------------------------
        // Низкоуровневое чтение файла
        // ----------------------------------------------------------------------------------------
        const auto fileContentOpt = FileLoader::loadTextFile(path);
        if (!fileContentOpt) {
            // Type A: обязательный контент. Без player.json нет смысла продолжать игру.
            LOG_PANIC(core::log::cat::Config,
                      "[ConfigLoader] Не удалось открыть обязательную конфигурацию игрока: {}",
                      path);
        }
        const std::string& fileContent = *fileContentOpt;

        // ----------------------------------------------------------------------------------------
        // Парсинг "критичного" конфига:
        //  - Отклонять дубликаты ключей
        //  - SchemaPolicy::Skip (никаких дублей; всё делает *WithIssue)
        // ----------------------------------------------------------------------------------------
        const Json data = json_utils::parseAndValidateCritical(
            fileContent,
            path,
            "ConfigLoader",
            {},
            json_utils::kConfigParseOnlyOptions);

        if (!data.is_object()) {
            LOG_PANIC(core::log::cat::Config,
                      "[ConfigLoader] Корневой JSON должен быть объектом. file={}",
                      path);
        }

        // Дефолты — из properties внутри PlayerBlueprint.
        blueprints::PlayerBlueprint cfg{};

        // ----------------------------------------------------------------------------------------
        // Текстура: строковый ID → enum TextureID (строго, Type A)
        // ----------------------------------------------------------------------------------------
        {
            const auto res = json_utils::parseEnumWithIssue(
                data,
                keys::Player::TEXTURE,
                rids::TextureID::Unknown,
                rids::textureFromString);

            using Kind = json_utils::EnumParseIssue::Kind;
            switch (res.issue.kind) {
            case Kind::None:
                cfg.sprite.textureId = res.value;
                break;

            case Kind::MissingKey:
                LOG_PANIC(core::log::cat::Config,
                          "[ConfigLoader] Обязательное поле '{}' отсутствует. file={}",
                          keys::Player::TEXTURE,
                          path);
                break;

            case Kind::WrongType:
                LOG_PANIC(core::log::cat::Config,
                          "[ConfigLoader] Некорректное поле '{}': {}. file={}",
                          keys::Player::TEXTURE,
                          json_utils::describe(res.issue),
                          path);
                break;

            case Kind::UnknownValue:
                LOG_PANIC(core::log::cat::Config,
                          "[ConfigLoader] Неизвестный texture ID '{}' (key='{}', file={})",
                          res.issue.raw,
                          keys::Player::TEXTURE,
                          path);
                break;

            default:
                LOG_PANIC(core::log::cat::Config,
                          "[ConfigLoader] Некорректное поле '{}': неизвестная ошибка. file={}",
                          keys::Player::TEXTURE,
                          path);
                break;
            }
        }

        // ----------------------------------------------------------------------------------------
        // Масштаб (строго, Type A)
        // ----------------------------------------------------------------------------------------
        {
            const auto scaleRes =
                json_utils::parseVec2fWithIssue(data, keys::Player::SCALE, cfg.sprite.scale);

            using Kind = json_utils::Vec2ParseIssue::Kind;
            if (scaleRes.issue.kind != Kind::None) {
                LOG_PANIC(core::log::cat::Config,
                          "[ConfigLoader] Некорректное поле '{}': {} (file={})",
                          keys::Player::SCALE,
                          json_utils::describe(scaleRes.issue),
                          path);
            }

            cfg.sprite.scale = scaleRes.value;
        }

        // ----------------------------------------------------------------------------------------
        // Движение (строго, Type A)
        // ----------------------------------------------------------------------------------------
        auto readRequiredFloat = [&](std::string_view key, float& dst) {
            const auto res = json_utils::parseFloatWithIssue(data, key, dst);

            using Kind = json_utils::FloatParseIssue::Kind;
            if (res.issue.kind != Kind::None) {
                LOG_PANIC(core::log::cat::Config,
                          "[ConfigLoader] Некорректное поле '{}': {} (file={})",
                          key,
                          json_utils::describe(res.issue),
                          path);
            }

            dst = res.value;
        };

        readRequiredFloat(keys::Player::SPEED, cfg.movement.maxSpeed);
        readRequiredFloat(keys::Player::ACCELERATION, cfg.movement.acceleration);
        readRequiredFloat(keys::Player::FRICTION, cfg.movement.friction);
        readRequiredFloat(keys::Player::TURN_RATE, cfg.aircraftControl.turnRateDegreesPerSec);

        // Начальный поворот (опциональное поле, если битое -> WARN + default)
        {
            const auto rotRes = json_utils::parseFloatWithIssue(
                data,
                keys::Player::INITIAL_ROTATION_DEGREES,
                cfg.aircraftControl.initialRotationDegrees);

            using Kind = json_utils::FloatParseIssue::Kind;
            if (rotRes.issue.kind == Kind::None) {
                cfg.aircraftControl.initialRotationDegrees = rotRes.value;
            } else if (rotRes.issue.kind != Kind::MissingKey) {
                LOG_WARN(core::log::cat::Config,
                         "[ConfigLoader] Некорректное поле '{}': {}. "
                         "Применено значение по умолчанию ({}). file={}",
                         keys::Player::INITIAL_ROTATION_DEGREES,
                         json_utils::describe(rotRes.issue),
                         cfg.aircraftControl.initialRotationDegrees,
                         path);
            }
        }

        // ----------------------------------------------------------------------------------------
        // Anchor / resize / lock (мягко: missing -> default; 
        //                         неверный или неизвестный типа -> WARN + default)
        // ----------------------------------------------------------------------------------------
        applyOptionalEnumWithWarn(data,
                                 keys::Player::ANCHOR,
                                 cfg.anchor.anchorType,
                                 ui_ids::tryParseAnchor,
                                 path);

        // start_position: поле опциональное. Если задано и битое -> WARN + default.
        {
            const auto posRes = json_utils::parseVec2fWithIssue(
                data,
                keys::Player::START_POSITION,
                cfg.anchor.startPosition);

            using Kind = json_utils::Vec2ParseIssue::Kind;
            if (posRes.issue.kind == Kind::None) {
                cfg.anchor.startPosition = posRes.value;
            } else if (posRes.issue.kind != Kind::MissingKey) {
                LOG_WARN(core::log::cat::Config,
                         "[ConfigLoader] Некорректное поле '{}': {}. "
                         "Применено значение по умолчанию. file={}",
                         keys::Player::START_POSITION,
                         json_utils::describe(posRes.issue),
                         path);
            }
        }

        applyOptionalEnumWithWarn(data,
                                 keys::Player::RESIZE_SCALING,
                                 cfg.anchor.scalingBehavior,
                                 ui_ids::tryParseScaling,
                                 path);

        applyOptionalEnumWithWarn(data,
                                 keys::Player::LOCK_BEHAVIOR,
                                 cfg.anchor.lockBehavior,
                                 ui_ids::tryParseLock,
                                 path);

        // ----------------------------------------------------------------------------------------
        // Управляющие клавиши
        //
        // Правило:
        //  - если ключ отсутствует -> остаётся дефолт (из cfg.controls);
        //  - если тип неверный / имя клавиши неизвестно / дубли -> 1x WARN
        // ----------------------------------------------------------------------------------------
        if (const auto controlsIt = data.find(keys::Player::CONTROLS); controlsIt != data.end()) {
            if (!controlsIt->is_object()) {
                LOG_WARN(core::log::cat::Config,
                         "[ConfigLoader] Некорректный блок '{}': ожидался объект. "
                         "Controls не применены, используются значения по умолчанию. file={}",
                         keys::Player::CONTROLS, path);
            } else {
                const auto& c = *controlsIt;

                const auto defaults = cfg.controls;
                auto parsed = defaults;

                bool hasHardError = false;

                auto applyKeyOptional = [&](std::string_view controlKey,
                                            sf::Keyboard::Key& dst) {
                    const auto res = json_utils::parseKeyWithIssue(c, controlKey, dst);

                    using Kind = json_utils::KeyParseIssue::Kind;
                    switch (res.issue.kind) {
                    case Kind::None:
                        dst = res.value;
                        return;

                    case Kind::MissingKey:
                        return;

                    case Kind::WrongType:
                        hasHardError = true;
                        LOG_DEBUG(core::log::cat::Config,
                                  "[ConfigLoader] controls.'{}': ожидалась строка. file={}",
                                  controlKey,
                                  path);
                        return;

                    case Kind::UnknownName:
                        hasHardError = true;
                        LOG_DEBUG(core::log::cat::Config,
                                  "[ConfigLoader] controls.'{}': неизвестная клавиша '{}'. file={}",
                                  controlKey,
                                  res.rawName,
                                  path);
                        return;

                    default:
                        hasHardError = true;
                        LOG_DEBUG(core::log::cat::Config,
                                  "[ConfigLoader] controls.'{}': неизвестная ошибка. file={}",
                                  controlKey,
                                  path);
                        return;
                    }
                };

                applyKeyOptional(keys::Player::CONTROL_THRUST_FORWARD, parsed.thrustForward);
                applyKeyOptional(keys::Player::CONTROL_THRUST_BACKWARD, parsed.thrustBackward);
                applyKeyOptional(keys::Player::CONTROL_TURN_LEFT, parsed.turnLeft);
                applyKeyOptional(keys::Player::CONTROL_TURN_RIGHT, parsed.turnRight);

                const auto layoutIssue = validateControlsLayout(parsed.thrustForward,
                                                                parsed.thrustBackward,
                                                                parsed.turnLeft,
                                                                parsed.turnRight);

                if (hasHardError || layoutIssue.kind != ControlsLayoutIssue::Kind::None) {
                    const char* reason = "неизвестная причина";

                    switch (layoutIssue.kind) {
                    case ControlsLayoutIssue::Kind::None:
                        reason = hasHardError ? "некорректные значения/типы" : "неизвестная причина";
                        break;
                    case ControlsLayoutIssue::Kind::Unknown:
                        reason = "обнаружен Unknown";
                        break;
                    case ControlsLayoutIssue::Kind::Duplicate:
                        reason = "обнаружены дубли";
                        break;
                    default:
                        break;
                    }

                    LOG_WARN(core::log::cat::Config,
                             "[ConfigLoader] Некорректный блок '{}': {}. "
                             "Controls не применены, используются значения по умолчанию. file={}",
                             keys::Player::CONTROLS,
                             reason,
                             path);

                    LOG_DEBUG(core::log::cat::Config,
                              "[ConfigLoader] controls defaults/applied: "
                              "(thrust_forward={}, thrust_backward={}, turn_left={}, turn_right={}) -> "
                              "(thrust_forward={}, thrust_backward={}, turn_left={}, turn_right={}). "
                              "file={}",
                              static_cast<int>(defaults.thrustForward),
                              static_cast<int>(defaults.thrustBackward),
                              static_cast<int>(defaults.turnLeft),
                              static_cast<int>(defaults.turnRight),
                              static_cast<int>(parsed.thrustForward),
                              static_cast<int>(parsed.thrustBackward),
                              static_cast<int>(parsed.turnLeft),
                              static_cast<int>(parsed.turnRight),
                              path);
                } else {
                    cfg.controls = parsed;
                }
            }
        }

        return cfg;
    }

} // namespace game::skyguard::config