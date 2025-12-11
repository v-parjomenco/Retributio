# **SkyGuard** *(name subject to change)*

## 📁 **Project Structure**

High-level layout of the SFML1 / SkyGuard repository.  

```text
.
├─ CREDITS.md
├─ README.md
├─ STRUCTURE.md
├─ SFML1.sln
├─ docs/
│  └─ architecture/           # Architecture notes & diagrams
├─ tools/
│  ├─ check_layering.ps1      # Include layering checks
│  └─ run-clang-tidy.ps1      # clang-tidy helper
└─ SFML1/
   ├─ assets/
   │  ├─ core/
   │  │  ├─ config/           # Engine configs (engine_settings.json, debug_overlay.json)
   │  │  └─ fonts/            # Engine fonts
   │  └─ game/
   │     └─ skyguard/
   │        ├─ config/        # SkyGuard configs (player.json, skyguard_game.json, resources.json)
   │        ├─ images/        # SkyGuard textures & sprites
   │        └─ sounds/        # SkyGuard sounds (placeholder for future SFX)
   ├─ include/
   │  ├─ core/
   │  │  ├─ compiler/         # warnings.h and platform glue (windows.h)
   │  │  ├─ config/
   │  │  │  ├─ blueprints/    # Data models for configs (e.g. debug overlay)
   │  │  │  ├─ loader/        # Loaders that parse JSON into models
   │  │  │  └─ properties/    # Reusable property groups (sprite, movement, text, etc.)
   │  │  ├─ debug/            # Debug build flags & hotkeys
   │  │  ├─ ecs/
   │  │  │  ├─ components/    # Engine-level ECS components
   │  │  │  ├─ systems/       # Engine-level ECS systems
   │  │  │  └─ ...            # World, SystemManager, registry glue
   │  │  ├─ log/              # Logging API, levels, categories, macros
   │  │  ├─ resources/
   │  │  │  ├─ config/        # Per-resource-type config structures
   │  │  │  ├─ holders/       # Generic ResourceHolder
   │  │  │  ├─ ids/           # Strong resource IDs and helpers
   │  │  │  ├─ paths/         # ResourcePaths registry facade
   │  │  │  └─ types/         # Thin wrappers over SFML resources
   │  │  ├─ time/             # TimeService and fixed timestep config
   │  │  ├─ ui/
   │  │  │  ├─ ids/           # Enum/string helpers for UI identifiers
   │  │  │  └─ ...            # Anchor, lock and scaling behaviors
   │  │  └─ utils/
   │  │     ├─ file_loader.h  # Low-level file I/O
   │  │     └─ json/          # JSON helpers & validation
   │  ├─ game/
   │  │  └─ skyguard/
   │  │     ├─ config/
   │  │     │  ├─ blueprints/ # Player blueprint, etc.
   │  │     │  ├─ loader/     # SkyGuard config loaders
   │  │     │  └─ window_config.h
   │  │     ├─ ecs/
   │  │     │  ├─ components/ # Game-specific ECS components
   │  │     │  └─ systems/    # Game-specific ECS systems
   │  │     └─ game.h         # SkyGuard Game facade (composition root)
   │  ├─ pch.h                # Precompiled header
   │  ├─ nlohmann/			  # nlohmann-json
   │  └─ third_party/
   │     ├─ json_fwd.hpp      # Wrapper over <nlohmann/json_fwd.hpp> for lightweight forward declarations
   │     ├─ json_silent.hpp   # Wrapper over <nlohmann/json.hpp> with warning suppression for json internals
   │     └─ LICENSE.MIT       # Upstream license (nlohmann/json)
   ├─ src/
   │  ├─ core/                # Implementations mirroring include/core
   │  │  ├─ compiler/
   │  │  ├─ config/
   │  │  ├─ debug/
   │  │  ├─ ecs/
   │  │  ├─ log/
   │  │  ├─ resources/
   │  │  ├─ time/
   │  │  ├─ ui/
   │  │  └─ utils/
   │  ├─ game/
   │  │  └─ skyguard/         # Implementations mirroring include/game/skyguard
   │  └─ pch.cpp
   ├─ logs/                   # Runtime logs (per session)
   └─ main_skyguard.cpp       # Entry point, logging & safety-net wiring
```