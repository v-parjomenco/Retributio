# **SkyGuard** *(name subject to change)*

## 📁 **Project Structure**
```text
SFML1/
├─ docs/
│  └─ architecture/
├─ tools/
└─ SFML1/
   ├─ SFML1.vcxproj
   ├─ SFML1.vcxproj.filters
   ├─ SFML1.vcxproj.user
   ├─ assets/
   │  ├─ core/
   │  │  ├─ config/
   │  │  └─ fonts/
   │  └─ game/
   │     └─ skyguard/
   │        ├─ config/
   │        ├─ images/
   │        └─ sounds/
   ├─ include/
   │  ├─ core/
   │  │  ├─ compiler/
   │  │  │  ├─ platform/
   │  │  │  │  └─ windows.h
   │  │  │  └─ warnings.h
   │  │  ├─ config/
   │  │  │  ├─ blueprints/
   │  │  │  │  └─ debug_overlay_blueprint.h
   │  │  │  ├─ config_keys.h
   │  │  │  ├─ engine_settings.h
   │  │  │  ├─ loader/
   │  │  │  │  ├─ debug_overlay_loader.h
   │  │  │  │  └─ engine_settings_loader.h
   │  │  │  └─ properties/
   │  │  │     ├─ anchor_properties.h
   │  │  │     ├─ controls_properties.h
   │  │  │     ├─ movement_properties.h
   │  │  │     ├─ sprite_properties.h
   │  │  │     └─ text_properties.h
   │  │  ├─ debug/
   │  │  │  └─ debug_config.h
   │  │  ├─ ecs/
   │  │  │  ├─ component_storage.h
   │  │  │  ├─ components/
   │  │  │  │  ├─ keyboard_control_component.h
   │  │  │  │  ├─ lock_behavior_component.h
   │  │  │  │  ├─ movement_stats_component.h
   │  │  │  │  ├─ scaling_behavior_component.h
   │  │  │  │  ├─ sprite_component.h
   │  │  │  │  ├─ transform_component.h
   │  │  │  │  └─ velocity_component.h
   │  │  │  ├─ entity.h
   │  │  │  ├─ entity_manager.h
   │  │  │  ├─ registry.h
   │  │  │  ├─ system.h
   │  │  │  ├─ system_manager.h
   │  │  │  ├─ systems/
   │  │  │  │  ├─ debug_overlay_system.h
   │  │  │  │  ├─ input_system.h
   │  │  │  │  ├─ lock_system.h
   │  │  │  │  ├─ movement_system.h
   │  │  │  │  ├─ render_system.h
   │  │  │  │  └─ scaling_system.h
   │  │  │  └─ world.h
   │  │  ├─ log/
   │  │  │  ├─ log_categories.h
   │  │  │  ├─ log_defaults.h
   │  │  │  ├─ log_level.h
   │  │  │  ├─ log_macros.h
   │  │  │  └─ logging.h
   │  │  ├─ resources/
   │  │  │  ├─ config/
   │  │  │  │  ├─ font_resource_config.h
   │  │  │  │  ├─ sound_resource_config.h
   │  │  │  │  └─ texture_resource_config.h
   │  │  │  ├─ holders/
   │  │  │  │  ├─ resource_holder.h
   │  │  │  │  └─ resource_holder.inl
   │  │  │  ├─ ids/
   │  │  │  │  ├─ resource_id_utils.h
   │  │  │  │  └─ resource_ids.h
   │  │  │  ├─ paths/
   │  │  │  │  └─ resource_paths.h
   │  │  │  ├─ resource_manager.h
   │  │  │  └─ types/
   │  │  │     ├─ font_resource.h
   │  │  │     ├─ soundbuffer_resource.h
   │  │  │     └─ texture_resource.h
   │  │  ├─ time/
   │  │  │  ├─ time_config.h
   │  │  │  └─ time_service.h
   │  │  ├─ ui/
   │  │  │  ├─ anchor_policy.h
   │  │  │  ├─ anchor_utils.h
   │  │  │  ├─ ids/
   │  │  │  │  └─ ui_id_utils.h
   │  │  │  ├─ lock_behavior.h
   │  │  │  └─ scaling_behavior.h
   │  │  └─ utils/
   │  │     ├─ file_loader.h
   │  │     └─ json/
   │  │        ├─ json_utils.h
   │  │        └─ json_validator.h
   │  ├─ game/
   │  │  └─ skyguard/
   │  │     ├─ config/
   │  │     │  ├─ blueprints/
   │  │     │  │  └─ player_blueprint.h
   │  │     │  ├─ config_keys.h
   │  │     │  ├─ loader/
   │  │     │  │  ├─ config_loader.h
   │  │     │  │  └─ window_config_loader.h
   │  │     │  └─ window_config.h
   │  │     ├─ ecs/
   │  │     │  ├─ components/
   │  │     │  │  └─ player_config_component.h
   │  │     │  └─ systems/
   │  │     │     └─ player_init_system.h
   │  │     └─ game.h
   │  ├─ pch.h
   │  └─ third_party/
   │     ├─ json.hpp
   │     └─ json_silent.hpp
   ├─ src/
   │  ├─ core/
   │  │  ├─ compiler/
   │  │  │  └─ platform/
   │  │  ├─ config/
   │  │  │  ├─ blueprints/
   │  │  │  ├─ loader/
   │  │  │  │  ├─ debug_overlay_loader.cpp
   │  │  │  │  └─ engine_settings_loader.cpp
   │  │  │  └─ properties/
   │  │  ├─ debug/
   │  │  ├─ ecs/
   │  │  │  ├─ components/
   │  │  │  └─ systems/
   │  │  │     ├─ debug_overlay_system.cpp
   │  │  │     ├─ movement_system.cpp
   │  │  │     └─ render_system.cpp
   │  │  ├─ log/
   │  │  │  ├─ log_level.cpp
   │  │  │  └─ logging.cpp
   │  │  ├─ resources/
   │  │  │  ├─ config/
   │  │  │  ├─ holders/
   │  │  │  ├─ ids/
   │  │  │  │  └─ resource_ids.cpp
   │  │  │  ├─ paths/
   │  │  │  │  └─ resource_paths.cpp
   │  │  │  ├─ resource_manager.cpp
   │  │  │  └─ types/
   │  │  ├─ time/
   │  │  │  └─ time_service.cpp
   │  │  ├─ ui/
   │  │  │  ├─ anchor_policy.cpp
   │  │  │  ├─ ids/
   │  │  │  │  └─ ui_id_utils.cpp
   │  │  │  ├─ lock_behavior.cpp
   │  │  │  └─ scaling_behavior.cpp
   │  │  └─ utils/
   │  │     ├─ file_loader.cpp
   │  │     └─ json/
   │  │        └─ json_utils.cpp
   │  ├─ game/
   │  │  └─ skyguard/
   │  │     ├─ config/
   │  │     │  ├─ blueprints/
   │  │     │  └─ loader/
   │  │     │     ├─ config_loader.cpp
   │  │     │     └─ window_config_loader.cpp
   │  │     ├─ ecs/
   │  │     │  ├─ components/
   │  │     │  └─ systems/
   │  │     └─ game.cpp
   │  └─ pch.cpp
   └─ main_skyguard.cpp
```