#include <fstream>
#include <stdexcept>
#include "third_party/json_silent.hpp"
#include "core/config_loader.h"
#include "core/config.h"

using json = nlohmann::json;

namespace core {

    // Функция для парсинга масштаба из JSON.
    // Принимает число, н-р: 0.12, массив: [0.12, 0.20] или объект: {"x": 0.12, "y": 0.20}
static sf::Vector2f parseScale(const json& scaleData) {
    if (scaleData.is_number()) {
        float s = scaleData.get<float>();
        return { s, s };
    }
    if (scaleData.is_array() && scaleData.size() >= 2) {
        return { scaleData[0].get<float>(), scaleData[1].get<float>() };
    }
    if (scaleData.is_object()) {
        float x = scaleData.value("x", 1.0f);
        float y = scaleData.value("y", 1.0f);
        return { x, y };
    }
    return { 1.f, 1.f };
}
    // Загружает JSON из файла и парсит PlayerConfig
PlayerConfig ConfigLoader::loadPlayerConfig(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open())
        throw std::runtime_error("ConfigLoader: не могу открыть файл " + path); // Не ту ошибку выдает (см. диалог)

    json data;
    file >> data;

    PlayerConfig cfg;
    cfg.texture = data.value("texture", cfg.texture);

    if (data.contains("scale")) {
        cfg.scale = parseScale(data["scale"]);
    }

    // speed: если скорость в JSON-файле отсутствует, сохраняем значением по умолчанию
    cfg.speed = data.value("speed", config::PLAYER_SPEED);

    if (data.contains("start_position")) {
        const auto& pos = data["start_position"];
        if (pos.is_object()) {
            cfg.startPosition.x = pos.value("x", 100.f);
            cfg.startPosition.y = pos.value("y", 100.f);
        }
        else if (pos.is_array() && pos.size() >= 2) {
            cfg.startPosition.x = pos[0].get<float>();
            cfg.startPosition.y = pos[1].get<float>();
        }
    }

    return cfg;
}

} // namespace core