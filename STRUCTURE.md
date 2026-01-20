# **SkyGuard** *(name subject to change)*

## 📁 **Project Structure**

High-level layout of the SFML1 / SkyGuard repository.

```text
.
├─ CREDITS.md
├─ README.md
├─ SFML1.sln
├─ STRUCTURE.md
├─ docs/
│  └─ architecture/              # Architecture notes & diagrams
├─ tools/
│  ├─ check_layering.ps1         # Include layering checks
│  └─ run-clang-tidy.ps1         # clang-tidy helper
└─ SFML1/
   ├─ SFML1.vcxproj
   ├─ SFML1.vcxproj.filters
   ├─ SFML1.vcxproj.user
   ├─ main_skyguard.cpp          # Entry point, logging & safety-net wiring
   ├─ assets/
   │  ├─ core/
   │  │  ├─ config/              # Engine configs (engine_settings.json, debug_overlay.json)
   │  │  └─ fonts/               # Engine fonts
   │  └─ game/
   │     └─ skyguard/
   │        ├─ config/           # SkyGuard configs (player.json, skyguard_game.json, resources.json)
   │        ├─ images/           # SkyGuard textures & sprites
   │        └─ sounds/           # SkyGuard sounds (placeholder / future SFX)
   ├─ include/
   │  ├─ pch.h                   # Precompiled header
   │  ├─ core/
   │  │  ├─ compiler/            # warnings.h and platform glue (windows.h)
   │  │  ├─ config/
   │  │  │  ├─ blueprints/       # Data models for configs (e.g. debug overlay)
   │  │  │  ├─ loader/           # Loaders that parse JSON into models
   │  │  │  └─ properties/       # Reusable property groups (sprite, movement, text, etc.)
   │  │  ├─ debug/               # Debug build flags & config
   │  │  ├─ ecs/
   │  │  │  ├─ components/       # Engine-level ECS components (POD)
   │  │  │  ├─ systems/          # Engine-level ECS systems
   │  │  │  ├─ entity.h
   │  │  │  ├─ system.h
   │  │  │  ├─ system_manager.h
   │  │  │  └─ world.h
   │  │  ├─ log/                 # Logging API, levels, categories, macros
   │  │  ├─ resources/
   │  │  │  ├─ config/           # Per-resource-type config structures
   │  │  │  ├─ holders/          # Generic ResourceHolder (dynamic paths/tools)
   │  │  │  ├─ keys/             # RuntimeKey32 types and helpers
   │  │  │  ├─ registry/         # ResourceRegistry (canonical keys -> configs)
   │  │  │  └─ types/            # Thin wrappers over SFML resources
   │  │  ├─ time/                # TimeService and fixed timestep config
   │  │  ├─ ui/                  # Anchor, lock and scaling behaviors
   │  │  └─ utils/
   │  │     ├─ file_loader.h     # Low-level file I/O
   │  │     └─ json/             # JSON parsing + validation helpers
   │  │        ├─ json_common.h
   │  │        ├─ json_validator.h
   │  │        ├─ json_document.h # parseAndValidateCritical/NonCritical
   │  │        ├─ json_accessors.h
   │  │        └─ json_parsers.h
   │  ├─ game/
   │  │  └─ skyguard/
   │  │     ├─ game.h            # SkyGuard Game façade (composition root)
   │  │     ├─ config/
   │  │     │  ├─ config_keys.h
   │  │     │  ├─ config_paths.h
   │  │     │  ├─ window_config.h
   │  │     │  ├─ blueprints/
   │  │     │  └─ loader/
   │  │     ├─ ecs/
   │  │     │  ├─ components/    # Game-specific ECS components (currently empty placeholder)
   │  │     │  └─ systems/       # Game-specific ECS systems
   │  │     └─ dev/
   │  │        └─ stress_scene.h
   │  ├─ entt/                   # Vendored EnTT headers
   │  ├─ nlohmann/               # Vendored nlohmann/json
   │  └─ third_party/            # Thin wrappers + licenses
   ├─ src/
   │  ├─ pch.cpp
   │  ├─ core/                   # Implementations mirroring include/core
   │  │  ├─ config/loader/
   │  │  ├─ ecs/systems/
   │  │  ├─ log/
   │  │  ├─ resources/
   │  │  ├─ time/
   │  │  ├─ ui/
   │  │  └─ utils/
   │  │     └─ json/
   │  └─ game/
   │     └─ skyguard/            # Implementations mirroring include/game/skyguard
   └─ logs/                      # Runtime logs (per session)
```