#pragma once
#include <string>

namespace core::keys {

    // Общие ключи, встречающиеся во многих JSON
    namespace Common {
        inline constexpr const char* TEXTURE = "texture";
        inline constexpr const char* SCALE = "scale";
        inline constexpr const char* SPEED = "speed";
        inline constexpr const char* ANCHOR = "anchor";
        inline constexpr const char* START_POSITION = "start_position";
        inline constexpr const char* RESIZE_BEHAVIOR = "resize_behavior";
    }

    // Ключи для конфигурации игрока
    namespace Player {
        inline constexpr const char* TEXTURE = Common::TEXTURE;
        inline constexpr const char* SCALE = Common::SCALE;
        inline constexpr const char* SPEED = Common::SPEED;
        inline constexpr const char* ANCHOR = Common::ANCHOR;
        inline constexpr const char* START_POSITION = Common::START_POSITION;
        inline constexpr const char* RESIZE_BEHAVIOR = Common::RESIZE_BEHAVIOR;
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

} // namespace core::keys