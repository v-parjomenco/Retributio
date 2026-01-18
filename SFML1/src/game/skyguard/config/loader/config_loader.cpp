#include "pch.h"

#include "game/skyguard/config/loader/config_loader.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

#include <SFML/Window/Keyboard.hpp>

#include "core/log/log_macros.h"
#include "core/resources/resource_manager.h"
#include "core/utils/file_loader.h"
#include "core/utils/json/json_document.h"
#include "core/utils/json/json_parsers.h"

#include "game/skyguard/config/config_keys.h"

namespace {

    using core::utils::FileLoader;

    namespace keys       = game::skyguard::config::keys;
    namespace json_utils = core::utils::json;

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

    // Вынесенная логика парсинга блока controls.
    // Шаблон используется для независимости от конкретной структуры ControlsT
    // (тип выводится из значений по умолчанию).
    template <typename ControlsT>
    [[nodiscard]] ControlsT parseControlsBlock(const Json& rootData,
                                               const ControlsT& defaults,
                                               std::string_view path)
    {
        const auto controlsIt = rootData.find(keys::Player::CONTROLS);
        if (controlsIt == rootData.end()) {
            return defaults;
        }

        if (!controlsIt->is_object()) {
            LOG_WARN(core::log::cat::Config,
                     "[ConfigLoader] Некорректный блок '{}': ожидался объект. "
                     "Controls не применены, используются значения по умолчанию. file={}",
                     keys::Player::CONTROLS,
                     path);
            return defaults;
        }

        const auto& c = *controlsIt;
        auto parsed = defaults;
        bool hasHardError = false;

        auto applyKeyOptional = [&](std::string_view controlKey, sf::Keyboard::Key& dst) {
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

        // Контракт: любые ошибки (тип/имя/дубликаты) -> полный откат к default controls.
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

            return defaults;
        }

        return parsed;
    }

} // namespace

namespace game::skyguard::config {

    blueprints::PlayerBlueprint ConfigLoader::loadPlayerConfig(
        core::resources::ResourceManager& resources,
        const std::string& path) {

        // ----------------------------------------------------------------------------------------
        // Низкоуровневое чтение файла
        // ----------------------------------------------------------------------------------------
        const auto fileContentOpt = FileLoader::loadTextFile(path);
        if (!fileContentOpt) {
            // Тип А: обязательный контент. Без player.json нет смысла продолжать игру.
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
        // Текстура: строковый ID → TextureKey (строго, тип А)
        // ----------------------------------------------------------------------------------------
        {
            const auto textureName =
                json_utils::parseStringViewWithIssue(data, keys::Player::TEXTURE);

            using Kind = json_utils::StringViewParseIssue::Kind;
            switch (textureName.issue.kind) {
            case Kind::None: {
                const core::resources::TextureKey key =
                    resources.findTexture(textureName.value);
                if (!key.valid()) {
                    LOG_PANIC(core::log::cat::Config,
                              "[ConfigLoader] Неизвестная текстура '{}': "
                              "поле '{}' не найдено в реестре. file={}",
                              textureName.value,
                              keys::Player::TEXTURE,
                              path);
                }
                cfg.sprite.texture = key;
                break;
            }
            case Kind::MissingKey:
                LOG_PANIC(core::log::cat::Config,
                          "[ConfigLoader] Обязательное поле '{}' отсутствует. file={}",
                          keys::Player::TEXTURE,
                          path);
                break;

            case Kind::WrongType:
                LOG_PANIC(core::log::cat::Config,
                          "[ConfigLoader] Некорректное поле '{}': ожидается строка. file={}",
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
        // Масштаб (строго, тип А)
        // Семантическая валидация: scale должен быть строго > 0, иначе invisible/degenerate.
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

            // Семантическая валидация: scale.x и scale.y должны быть строго > 0.
            // Используем !(x > 0) для NaN-safe проверки.
            if (!(cfg.sprite.scale.x > 0.0f) || !(cfg.sprite.scale.y > 0.0f)) {
                LOG_PANIC(core::log::cat::Config,
                          "[ConfigLoader] Некорректное поле '{}': "
                          "значения должны быть > 0.0 (x={}, y={}). file={}",
                          keys::Player::SCALE,
                          cfg.sprite.scale.x,
                          cfg.sprite.scale.y,
                          path);
            }
        }

        // ----------------------------------------------------------------------------------------
        // Движение (строго, тип А)
        // Атомарная операция: парсинг + семантическая валидация.
        // Используем !(x >= 0.0f) для NaN-safe проверки: IEEE 754 гарантирует, что любое
        // сравнение с NaN возвращает false, поэтому !(NaN >= 0.0f) == true.
        // Validate on write, trust on read — проверка только на границе загрузки.
        // ----------------------------------------------------------------------------------------
        auto readRequiredNonNegativeFloat = [&](std::string_view key, float& dst) {
            // 1. Парсинг JSON
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

            // 2. Семантическая валидация (NaN-safe, >= 0.0)
            if (!(dst >= 0.0f)) {
                LOG_PANIC(core::log::cat::Config,
                          "[ConfigLoader] Некорректное поле '{}': "
                          "значение должно быть >= 0.0. file={}",
                          key,
                          path);
            }
        };

        readRequiredNonNegativeFloat(
            keys::Player::SPEED, cfg.movement.maxSpeed);
        readRequiredNonNegativeFloat(
            keys::Player::ACCELERATION, cfg.movement.acceleration);
        readRequiredNonNegativeFloat(
            keys::Player::FRICTION, cfg.movement.friction);
        readRequiredNonNegativeFloat(
            keys::Player::TURN_RATE, cfg.aircraftControl.turnRateDegreesPerSec);

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
        // start_position: поле опциональное. Если задано и битое -> WARN + default.
        // ----------------------------------------------------------------------------------------
        {
            const auto posRes = json_utils::parseVec2fWithIssue(
                data,
                keys::Player::START_POSITION,
                cfg.startPosition);

            using Kind = json_utils::Vec2ParseIssue::Kind;
            if (posRes.issue.kind == Kind::None) {
                cfg.startPosition = posRes.value;
            } else if (posRes.issue.kind != Kind::MissingKey) {
                LOG_WARN(core::log::cat::Config,
                         "[ConfigLoader] Некорректное поле '{}': {}. "
                         "Применено значение по умолчанию. file={}",
                         keys::Player::START_POSITION,
                         json_utils::describe(posRes.issue),
                         path);
            }
        }

        // ----------------------------------------------------------------------------------------
        // Управляющие клавиши (через вынесенный хелпер)
        // ----------------------------------------------------------------------------------------
        cfg.controls = parseControlsBlock(data, cfg.controls, path);

        return cfg;
    }

} // namespace game::skyguard::config