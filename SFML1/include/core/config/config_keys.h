// ================================================================================================
// File: core/config/config_keys.h
// Purpose: Centralized keys for JSON configurations. Used by all parsing systems.
// Used by: DebugOverlayLoader, UI, ECS, future core config loaders
// Related headers: debug_overlay_loader.h
// ================================================================================================
#pragma once

#include <string>

namespace core::config::keys {

    // --------------------------------------------------------------------------------------------
    // Общие ключи, встречающиеся во многих JSON (движковый уровень)
    // --------------------------------------------------------------------------------------------
    namespace Common {
        inline constexpr const char* TEXTURE = "texture";
        inline constexpr const char* SCALE = "scale";
        inline constexpr const char* SPEED = "speed";
        inline constexpr const char* ACCELERATION = "acceleration";
        inline constexpr const char* FRICTION = "friction";
        inline constexpr const char* ANCHOR = "anchor";
        inline constexpr const char* START_POSITION = "start_position";
        inline constexpr const char* RESIZE_SCALING = "resize_scaling";
        inline constexpr const char* LOCK_BEHAVIOR = "lock_behavior";
        
        // Общие текстовые настройки
        inline constexpr const char* COLOR = "color";
        inline constexpr const char* CHARACTER_SIZE = "characterSize";
        inline constexpr const char* FONT = "font";
        
    } // namespace Common

    // --------------------------------------------------------------------------------------------
    // Ключи для движкового конфига EngineSettings (assets/core/config/engine_settings.json)
    // --------------------------------------------------------------------------------------------
    namespace EngineSettings {
        inline constexpr const char* VSYNC = "vsync";
        inline constexpr const char* FRAME_LIMIT = "frameLimit";
    } // namespace EngineSettings

    // --------------------------------------------------------------------------------------------
    // Ключи настроек текста для debug overlay (assets/core/config/debug_overlay.json)
    // --------------------------------------------------------------------------------------------
    namespace DebugOverlay {
        inline constexpr const char* ENABLED = "enabled";
        inline constexpr const char* POSITION = "position";
        
        // Используем общие текстовые ключи, чтобы не дублировать литералы
        inline constexpr const char* CHARACTER_SIZE = Common::CHARACTER_SIZE;
        inline constexpr const char* COLOR = Common::COLOR;
        
    } // namespace DebugOverlay

    // --------------------------------------------------------------------------------------------
    // Ключи для интерфейса (UI-конфиги, на будущее)
    // --------------------------------------------------------------------------------------------
    namespace UI {
        
        // Для UI тоже опираемся на те же общие ключи
        inline constexpr const char* FONT = Common::FONT;
        inline constexpr const char* CHARACTER_SIZE = Common::CHARACTER_SIZE;
        inline constexpr const char* COLOR = Common::COLOR;
        
    } // namespace UI

    // --------------------------------------------------------------------------------------------
    // Ключи для мира (terrain, lighting, weather и т.д., на будущее)
    // --------------------------------------------------------------------------------------------
    namespace World {
        inline constexpr const char* MAP_SIZE = "map_size";
        inline constexpr const char* GRAVITY = "gravity";
        inline constexpr const char* BACKGROUND = "background";
    } // namespace World

    // --------------------------------------------------------------------------------------------
    // Ключи для будущих конфигураций сущностей (Entity configs)
    // --------------------------------------------------------------------------------------------
    namespace Entity {
        inline constexpr const char* NAME = "name";
        inline constexpr const char* TYPE = "type";
        inline constexpr const char* COMPONENTS = "components";
    } // namespace Entity

} // namespace core::config::keys