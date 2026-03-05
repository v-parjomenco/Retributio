# Retributio Engine

A high-performance, modular 2D game engine built with **C++20 + SFML 3.0.2**, designed to scale from casual games to large-scale 4X strategy with hundreds of thousands of entities.

---

## Vision

**Retributio Engine** targets "indie-AAA" quality — production-grade architecture optimized for 2D and small teams.

### Games
- **Atrapacielos** — aerial combat / casual game (current focus)
- **Auctoritas** — 4X grand strategy (Civilization / Europa Universalis scale, planned)

---

## Core Features

- **Strict Layered Architecture** — Reusable engine core (`engine/`) cleanly separated from game-specific code (`games/`)
- **Data-Driven Design** — All gameplay, configs, and resources defined in JSON; zero hardcoded magic constants
- **Entity–Component–System (ECS)** — Performance-focused ECS with EnTT backend, designed for 100K+ entities
- **Key-Based Resource Management** — RuntimeKey32 handles, centralized registry, automatic caching and validation
- **Resolution-Independent UI** — Anchor policies, scaling behaviors, and lock systems for adaptive layouts
- **Deterministic Simulation** — Fixed timestep, stable iteration order, seeded RNG for replay and multiplayer support
- **Production-Grade Tooling** — Structured logging, debug overlay, profiling hooks, spatial stress harness

---

## Design Principles

1. **Data-Driven First** — Behavior expressed through configs and blueprints, not inheritance hierarchies
2. **ECS Over OOP** — Components are pure data, systems are pure logic, no virtual gameplay classes
3. **Separation of Concerns** — Core stays game-agnostic; simulation ≠ rendering
4. **Performance Over Beauty** — Target: 60 FPS with hundreds of thousands of active entities
5. **Fail-Fast Validation** — Invalid configs, broken registries, and impossible invariants terminate immediately
6. **Incremental Complexity** — Start simple, add features as profiling reveals bottlenecks

---

## Architecture Overview

### ECS Core (`core::ecs`)
- **Entity** — Lightweight numeric ID
- **Component** — Plain data structs (Transform, Velocity, Sprite, etc.)
- **System** — Logic operating over entity views (Movement, Rendering, Input, etc.)
- **World** — Thin façade over EnTT registry coordinating entities and systems

### Resource Layer (`core::resources`)
```
ResourceManager (façade)
    ↓
ResourceRegistry (CanonicalKey → Config + RuntimeKey32)
    ↓
Vector caches (per-type, indexed by RuntimeKey32)
    ↓
*Resource wrappers (Texture, Font, SoundBuffer)
    ↓
SFML primitives
```

**Key Principle:** Resources accessed via RuntimeKey32 handles (`TextureKey`, `FontKey`, `SoundKey`),
never raw strings or SFML types in gameplay code.

### Configuration Pipeline
```
JSON files → FileLoader → JsonValidator → Typed Blueprints/Settings → Systems
```

All configs validated on load; failures are logged and terminate early.

### Time & Main Loop
- `TimeService` — Fixed timestep updates, frame timing, FPS metrics
- `Game` — Composition root owning World, ResourceManager, and the main loop
- Deterministic simulation via stable `dt`, ordered system execution, and seeded RNG

---

## Project Structure

```text
Retributio/
├── engine/
│   ├── include/                 # Public engine headers (core/, adapters/, pch.h)
│   ├── include_private/         # Backend-specific implementation headers
│   ├── src/core/                # Engine implementations
│   ├── assets/                  # Engine assets (config, fonts, placeholders)
│   └── cmake/                   # CMake helpers (run_with_env.cmake)
├── games/
│   ├── atrapacielos/
│   │   ├── include/             # Game headers
│   │   ├── src/                 # Game implementations
│   │   └── assets/              # Game assets (textures, sounds, configs)
│   └── auctoritas/
│       └── src/                 # Stub entry point
├── tools/
│   ├── src/                     # Spatial harness, render stress
│   └── presets/atrapacielos/    # Stress preset .env files
├── tests/
│   ├── engine/                  # Google Tests for engine layer
│   └── atrapacielos/            # Google Tests for game layer
├── third_party/                 # xxhash, EnTT, nlohmann-json
├── CMakeLists.txt               # Solution root
└── CMakePresets.json
```

---

## Build Instructions

### Prerequisites
- **Visual Studio 2022+** with C++ workload (MSVC 19.29+)
- **CMake 3.25+**
- **Ninja** (bundled with Visual Studio)
- **vcpkg** — manages all dependencies (SFML, GTest)

### First-time setup

```powershell
# Clone
git clone https://github.com/v-parjomenco/Retributio.git
cd Retributio

# Configure (vcpkg runs automatically via CMakePresets.json)
cmake --preset win-msvc-ninja-mc
```

### Build

```powershell
# Debug — /Od, dev overlay, console subsystem
cmake --build --preset win-debug --target retributio_atrapacielos_game

# Profile — /O2, dev overlay, no console
cmake --build --preset win-profile --target retributio_atrapacielos_game

# Release — /O2, no overlay, shipping
cmake --build --preset win-release --target retributio_atrapacielos_game
```

### Tests

```powershell
# Build + run
cmake --build --preset win-debug --target retributio_engine_tests retributio_atrapacielos_tests
ctest --preset win-debug

# Only failed
ctest --preset win-debug --rerun-failed --output-on-failure
```

### Stress tools

```powershell
# Build spatial harness
cmake --build --preset win-profile --target retributio_spatial_harness

# Run with preset (smoke / soak)
cmake --build --preset win-profile --target stress_small
cmake --build --preset win-profile --target stress_large
```

---

## Development Tools & Code Style

### Code Standards
- **C++20** — No legacy OOP hierarchies; ECS and data-driven patterns throughout
- **Indentation:** 4 spaces, no tabs
- **Braces:** Same line (`if (...) {`)
- **Naming:** `snake_case` files, `PascalCase` classes, `camelCase` members with `m` prefix
- **Include order:** PCH → STL → SFML → third-party → engine headers → game headers
- **Value initialization:** Always `Type obj{};` for Config/Blueprint/Settings/Component types
- **Comments:** File banner headers in English; all inline comments in Russian

### Static Analysis & Formatting

```powershell
# Run clang-tidy (all layers)
.\tools\run-clang-tidy.ps1

# Run clang-tidy (engine layer only)
.\tools\run-clang-tidy.ps1 -Layer engine

# Check layering (engine must not depend on games/)
.\tools\check_layering.ps1

# Format
clang-format -i $(git ls-files *.h *.cpp)
```

---

## Quick Start for Developers

### Adding a New Resource

1. **Add asset file**
   ```
   games/atrapacielos/assets/textures/ship.png
   ```

2. **Register in resources.json**
   ```json
   {
     "version": 1,
     "textures": {
       "atrapacielos.sprite.ship": {
         "path": "assets/textures/ship.png",
         "smooth": true,
         "repeated": false,
         "mipmap": false
       }
     }
   }
   ```

3. **Use in code**
   ```cpp
   const TextureKey key = resourceManager.findTexture("atrapacielos.sprite.ship");
   const sf::Texture& tex = resourceManager.getTexture(key);
   ```

### Creating a New ECS Component

1. **Define in `engine/include/core/ecs/components/`**
   ```cpp
   struct HealthComponent {
       int current{100};
       int maximum{100};
   };
   ```

2. **Use in systems**
   ```cpp
   auto view = world.view<HealthComponent, TransformComponent>();
   for (auto entity : view) {
       auto& health = view.get<HealthComponent>(entity);
       // ...
   }
   ```

### Creating a New ECS System

1. **Implement in `engine/include/core/ecs/systems/`**
   ```cpp
   class HealthRegenSystem : public ISystem {
   public:
       void update(World& world, float dt) override {
           auto view = world.view<HealthComponent>();
           for (auto entity : view) {
               auto& health = view.get<HealthComponent>(entity);
               if (health.current < health.maximum) {
                   health.current += static_cast<int>(10.0f * dt);
               }
           }
       }
   };
   ```

2. **Register in game initialization**
   ```cpp
   systemManager.addSystem<HealthRegenSystem>();
   ```

---

## Contributing Guidelines

1. **Follow ECS principles** — Components are data, systems are logic, no gameplay inheritance
2. **Keep engine game-agnostic** — `engine/` must never `#include` anything from `games/`
3. **Validate all configs** — Use project JSON accessors for all JSON parsing
4. **Log appropriately** — Use correct categories (`Engine`, `Config`, `Resources`, `Gameplay`, etc.)
5. **Value-initialize data types** — `PlayerBlueprint player{};` not `PlayerBlueprint player;`
6. **Test before committing** — Project must build and all tests pass after every change
7. **Avoid external dependencies** — Only add libraries when absolutely essential

---

## Roadmap

### Foundation (Current)
- [x] ECS framework with EnTT backend
- [x] Key-based resource registry (RuntimeKey32 / StableKey64)
- [x] Fixed timestep time service
- [x] Config-driven architecture with JSON validation
- [x] Debug overlay and structured logging
- [x] Spatial indexing (quad-tree V1 + V2)
- [x] Ninja Multi-Config CMake build (Debug / Profile / Release)
- [x] Google Tests foundation
- [ ] Hot-reload for JSON configs
- [ ] Stress testing with 10K+ entities

### Engine Maturity
- [ ] Asynchronous resource loading and streaming
- [ ] Event bus and scriptable triggers
- [ ] Save/load system with versioning
- [ ] Cross-platform CMake (Linux/macOS)

### Auctoritas (4X Grand Strategy)
- [ ] Layered simulation (tactical / operational / strategic)
- [ ] Data-driven AI (Utility AI + GOAP/HTN + Behavior Trees)
- [ ] Deterministic replay and lock-step multiplayer
- [ ] Delta saves and chunked loading
- [ ] Paradox-style event system (scriptable triggers/effects)
- [ ] Mod support and content pipeline

---

## Documentation

- **[Architecture Reference](./docs/architecture/)** — Deep-dive into engine design
- **[CMake Roadmap](./docs/CMake_Integration_ROADMAP.md)** — Build system phases and acceptance gates
- **Code Comments** — Inline Russian comments explain intent and invariants

---

## Licenses

- **Engine code:** License TBD
- **Third-party libraries:**
  - nlohmann-json — MIT License, see `third_party/licenses/json_license.mit`
  - EnTT — MIT License, see `third_party/licenses/entt_license.mit`
  - xxHash — BSD 2-Clause License, see `third_party/licenses/xxhash_license.txt`

---

**Built with passion for scalable, data-driven game architecture.**  
*Performance over beauty. Determinism over randomness. Code that lasts.*
