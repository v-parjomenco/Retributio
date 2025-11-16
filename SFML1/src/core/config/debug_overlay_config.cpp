#include "pch.h"
#include "core/config/debug_overlay_config.h"

#include <fstream>
#include <stdexcept>

#include "core/utils/json/json_utils.h"
#include "core/utils/json/json_validator.h"
#include "core/utils/message.h"

using core::utils::json::json;

namespace {

    sf::Color parseColor(const json& value, const sf::Color& fallback) {
        if (value.is_string()) {
            std::string s = value.get<std::string>();
            // Поддержка форматов: "#RRGGBB" / "#RRGGBBAA"
            if (!s.empty() && s[0] == '#') {
                s.erase(0, 1);
                auto hexTo = [](const std::string& h) -> uint8_t {
                    return static_cast<uint8_t>(std::stoul(h, nullptr, 16));
                };
                try {
                    if (s.size() == 6) {
                        return {hexTo(s.substr(0, 2)), hexTo(s.substr(2, 2)), hexTo(s.substr(4, 2)),
                                255};
                    } else if (s.size() == 8) {
                        return {hexTo(s.substr(0, 2)), hexTo(s.substr(2, 2)), hexTo(s.substr(4, 2)),
                                hexTo(s.substr(6, 2))};
                    }
                } catch (const std::exception& e) {
                    core::utils::message::logDebug(
                        std::string("[DebugOverlayConfig]\nНекорректное значение color: ") +
                        e.what());
                } catch (...) {
                    core::utils::message::logDebug(
                        "[DebugOverlayConfig]\nНекорректное значение color: неизвестная ошибка");
                }
            }
        } else if (value.is_object()) {
            auto r = static_cast<uint8_t>(value.value("r", 255));
            auto g = static_cast<uint8_t>(value.value("g", 255));
            auto b = static_cast<uint8_t>(value.value("b", 255));
            auto a = static_cast<uint8_t>(value.value("a", 255));
            return {r, g, b, a};
        }
        return fallback;
    }

} // namespace

namespace core::config {

    DebugOverlayConfig loadDebugOverlayConfig(const std::string& path) {
        DebugOverlayConfig cfg;
        std::ifstream ifs(path);
        if (!ifs) {
            core::utils::message::logDebug("[DebugOverlayConfig]\nФайл не найден: " + path);
            return cfg;
        }

        json data;
        try {
            ifs >> data;
        } catch (...) {
            core::utils::message::logDebug("[DebugOverlayConfig]\nНеверный JSON: " + path);
            return cfg;
        }

        using VR = core::utils::json::JsonValidator::KeyRule;
        try {
            core::utils::json::JsonValidator::validate(
                data, {{"enabled", {json::value_t::boolean}, false},
                       {"position",
                        {json::value_t::array, json::value_t::object, json::value_t::number_float},
                        false},
                       {"characterSize",
                        {json::value_t::number_integer, json::value_t::number_unsigned},
                        false},
                       {"color", {json::value_t::string, json::value_t::object}, false}});
        } catch (const std::exception& e) {
            core::utils::message::logDebug(std::string("[DebugOverlayConfig]\nВалидация: ") +
                                           e.what());
            return cfg;
        }

        cfg.enabled = data.value("enabled", cfg.enabled);
        cfg.position = core::utils::json::parseValue<sf::Vector2f>(data, "position", cfg.position);
        cfg.characterSize =
            static_cast<unsigned>(data.value("characterSize", static_cast<int>(cfg.characterSize)));
        if (data.contains("color")) {
            cfg.color = parseColor(data["color"], cfg.color);
        }

        return cfg;
    }

} // namespace core::config