#pragma once

#include <string>

namespace config::keys {

    // Общие ключи, встречающиеся во многих JSON
    namespace Common {
        inline constexpr const char* TEXTURE = "texture";
        inline constexpr const char* SCALE = "scale";
        inline constexpr const char* SPEED = "speed";
        inline constexpr const char* ACCELERATION = "acceleration";
        inline constexpr const char* FRICTION = "friction";
        inline constexpr const char* ANCHOR = "anchor";
        inline constexpr const char* START_POSITION = "start_position";
        inline constexpr const char* SCALING = "scaling";
        inline constexpr const char* LOCK_BEHAVIOR = "lock_behavior";
    }

    // Ключи для конфигурации игрока
    namespace Player {
        inline constexpr const char* TEXTURE = Common::TEXTURE;
        inline constexpr const char* SCALE = Common::SCALE;
        inline constexpr const char* SPEED = Common::SPEED;
        inline constexpr const char* ACCELERATION = Common::ACCELERATION;
        inline constexpr const char* FRICTION = Common::FRICTION;
        inline constexpr const char* ANCHOR = Common::ANCHOR;
        inline constexpr const char* START_POSITION = Common::START_POSITION;
        inline constexpr const char* SCALING = Common::SCALING;
        inline constexpr const char* LOCK_BEHAVIOR = Common::LOCK_BEHAVIOR;

        inline constexpr const char* CONTROLS = "controls";
        inline constexpr const char* CONTROL_UP = "up";
        inline constexpr const char* CONTROL_DOWN = "down";
        inline constexpr const char* CONTROL_LEFT = "left";
        inline constexpr const char* CONTROL_RIGHT = "right";
    }

    // Ключи для интерфейса (на будущее)
    namespace UI {
        inline constexpr const char* FONT = "font";
        inline constexpr const char* FONT_SIZE = "font_size";
        inline constexpr const char* COLOR = "color";
    }

    // Ключи для мира (terrain, lighting, weather и т.д., на будущее)
    namespace World {
        inline constexpr const char* MAP_SIZE = "map_size";
        inline constexpr const char* GRAVITY = "gravity";
        inline constexpr const char* BACKGROUND = "background";
    }

    // Ключи для будущих конфигураций сущностей (Entity configs)
    namespace Entity {
        inline constexpr const char* NAME = "name";
        inline constexpr const char* TYPE = "type";
        inline constexpr const char* COMPONENTS = "components";
    }

} // namespace config::keys

// -----------------------------------------------------------------------------
// config_keys.h
// Централизованные ключи для JSON-конфигураций.
// Используется всеми системами парсинга (ConfigLoader, UI, ECS и т.д.).
// -----------------------------------------------------------------------------