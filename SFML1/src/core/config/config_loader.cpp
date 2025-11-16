#include "pch.h"
#include "core/config/config_loader.h"

#include <cstdlib>
#include <fstream>
#include <stdexcept>

#include "third_party/json_silent.hpp"

#include "core/config/config_keys.h"
#include "core/utils/json/json_utils.h"
#include "core/utils/json/json_validator.h"
#include "core/utils/message.h"

using namespace ::core::utils::json;
using namespace ::core::utils::message;
using namespace core::config::keys;

namespace core::config {

    // Загружает JSON-конфигурацию игрока из файла
    PlayerConfig ConfigLoader::loadPlayerConfig(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            core::utils::message::showError(
                "[ConfigLoader]\nНе удалось открыть конфигурацию игрока: " + path +
                ".\nБудут использованы значения по умолчанию.");

            // Возвращаем дефолтный PlayerConfig без выбрасывания исключения
            return PlayerConfig{};
        }

        // JSON-данные
        json data;
        try {
            file >> data;
        } catch (const std::exception& e) {
            core::utils::message::showError(std::string("[ConfigLoader]\nОшибка чтения JSON: ") +
                                            e.what());
            std::exit(EXIT_FAILURE);
        }

        try {
            // Проверяем структуру JSON перед парсингом
            JsonValidator::validate(
                data,
                {{Player::TEXTURE, {json::value_t::string}},
                 {Player::SCALE,
                  {json::value_t::number_float, json::value_t::object, json::value_t::array}},
                 {Player::SPEED, {json::value_t::number_float}, false},
                 {Player::ACCELERATION, {json::value_t::number_float}, false},
                 {Player::FRICTION, {json::value_t::number_float}, false},
                 {Player::ANCHOR, {json::value_t::string}, false},
                 {Player::START_POSITION, {json::value_t::object, json::value_t::array}, false},
                 {Player::SCALING, {json::value_t::string}, false},
                 {Player::LOCK_BEHAVIOR, {json::value_t::string}, false},
                 {Player::CONTROLS, {json::value_t::object}, false}});
        } catch (const std::exception& e) {
            core::utils::message::showError(
                std::string("[ConfigLoader]\nНеверная структура JSON: ") + e.what());
            std::exit(EXIT_FAILURE);
        }
        // Конфигурация игрока на основе JSON-данных
        PlayerConfig
            cfg; // создаёт объект конфигурации игрока со значениями по по умолчанию из config.h

        // Загружает параметры из JSON с использованием ключей, из config_keys.h
        cfg.texturePath = parseValue<std::string>(data, Player::TEXTURE, cfg.texturePath);
        cfg.scale = parseValue<sf::Vector2f>(data, Player::SCALE, cfg.scale);
        cfg.speed = parseValue<float>(data, Player::SPEED, cfg.speed);
        cfg.acceleration = parseValue<float>(data, Player::ACCELERATION, cfg.acceleration);
        cfg.friction = parseValue<float>(data, Player::FRICTION, cfg.friction);
        cfg.anchor = parseValue<std::string>(data, Player::ANCHOR, cfg.anchor);
        cfg.startPosition =
            parseValue<sf::Vector2f>(data, Player::START_POSITION, cfg.startPosition);
        cfg.scaling = parseValue<std::string>(data, Player::SCALING, cfg.scaling);
        cfg.lockBehavior = parseValue<std::string>(data, Player::LOCK_BEHAVIOR, cfg.lockBehavior);

        // Controls (необязательный объект)
        if (data.contains(Player::CONTROLS) && data.at(Player::CONTROLS).is_object()) {
            const auto& c = data.at(Player::CONTROLS);

            // Каждая клавиша может быть переопределена в JSON
            cfg.controls.up = parseKey(c, Player::CONTROL_UP, cfg.controls.up);
            cfg.controls.down = parseKey(c, Player::CONTROL_DOWN, cfg.controls.down);
            cfg.controls.left = parseKey(c, Player::CONTROL_LEFT, cfg.controls.left);
            cfg.controls.right = parseKey(c, Player::CONTROL_RIGHT, cfg.controls.right);
        } else {
            core::utils::message::showError(
                "[ConfigLoader]\nБлок controls отсутствует или имеет неверный тип. "
                "Применены значения по умолчанию.");
        }

        if (cfg.scaling != "Uniform" && cfg.scaling != "None") {
            core::utils::message::showError(
                std::string("[ConfigLoader]\nНеизвестное значение scaling: ") + cfg.scaling +
                ". Применено значение по умолчанию (None).");
            cfg.scaling = "None";
        }

        if (cfg.lockBehavior != "ScreenLock" && cfg.lockBehavior != "WorldLock") {
            core::utils::message::showError(
                std::string("[ConfigLoader]\nНеизвестное значение lock_behavior: ") +
                cfg.lockBehavior + ". Применено значение по умолчанию (WorldLock).");
            cfg.lockBehavior = "WorldLock";
        }

        if (core::ui::anchors::fromString(cfg.anchor) == core::ui::AnchorType::None &&
            cfg.anchor != "None") {
            core::utils::message::showError(
                std::string("[ConfigLoader]\nНеизвестное значение anchor: ") + cfg.anchor +
                ". Применено значение по умолчанию (None).");
            cfg.anchor = "None";
        }

        return cfg;
    }

} // namespace core::config