#include "pch.h"

#include "core/config/debug_overlay_config.h"
#include <fstream>
#include <stdexcept>

#include "core/utils/json/json_utils.h"
#include "core/utils/json/json_validator.h"
#include "core/utils/message.h"

using core::utils::json::json;

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
        cfg.color = utils::json::parseColor(data, "color", cfg.color);

        return cfg;
    }

} // namespace core::config