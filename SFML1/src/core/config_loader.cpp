#include <fstream>
#include <stdexcept>
#include "third_party/json_silent.hpp"
#include "core/config.h"
#include "core/config_keys.h"
#include "core/config_loader.h"
#include "core/json_utils.h"
#include "core/json_validator.h"
#include "utils/message.h"

using namespace core::json_utils;
using namespace core::keys;

namespace core {

        // Загружает JSON-конфигурацию игрока из файла
    PlayerConfig ConfigLoader::loadPlayerConfig(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            utils::message::showError(
                "[ConfigLoader]\nНе удалось открыть конфигурацию игрока: " + path +
                ".\nБудут использованы значения по умолчанию."
            );

            // Возвращаем дефолтный PlayerConfig без выбрасывания исключения
            return PlayerConfig{};
        }

        // JSON-данные
        json data;
        file >> data;

        // Проверяем структуру JSON перед парсингом
        core::JsonValidator::validate(data, {
            { Player::TEXTURE,         { json::value_t::string } },
            { Player::SCALE,           { json::value_t::number_float, json::value_t::array } },
            { Player::SPEED,           { json::value_t::number_float } },
            { Player::ANCHOR,          { json::value_t::string }, false },
            { Player::START_POSITION,  { json::value_t::array, json::value_t::object }, false },
            { Player::RESIZE_BEHAVIOR, { json::value_t::string }, false }
            });

        // Конфигурация игрока на основе JSON-данных
        PlayerConfig cfg; // создаёт объект конфигурации игрока со значениями по по умолчанию

        // Загружает параметры из JSON с использованием ключей, из config_keys.h
        cfg.texturePath                    = parseValue<std::string>(data, Player::TEXTURE, cfg.texturePath);
        cfg.scale                      = parseValue<sf::Vector2f>(data, Player::SCALE, cfg.scale);
        cfg.speed                      = parseValue<float>(data, Player::SPEED, cfg.speed);
        cfg.startPosition              = parseValue<sf::Vector2f>(data, Player::START_POSITION, cfg.startPosition);
        cfg.resizeBehavior             = parseValue<std::string>(data, Player::RESIZE_BEHAVIOR, cfg.resizeBehavior);
        cfg.anchor                     = parseValue<std::string>(data, Player::ANCHOR, cfg.anchor);

        return cfg;
    }

} // namespace core