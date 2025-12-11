# **SkyGuard Engine**

A high-performance, modular 2D game engine built with **C++20 + SFML 3.0.2**, designed to scale from indie prototypes to large-scale 4X strategy games with hundreds of thousands of entities.

---

## 🎯 **Vision**

**SkyGuard** is architected as a universal 2D engine targeting "indie-AAA" quality — comparable in ambition to UE5 or Paradox's Clausewitz engine, but optimized for 2D and small teams.

### Development Phases
1. **Phase 1 (Current):** Aerial combat prototype as engine testbed
2. **Phase 2 (Year 1-2):** Multiple casual games validating engine flexibility
3. **Phase 3 (Year 3-5):** Full-scale 4X grand strategy game (Civilization/Europa Universalis level)

---

## 🚀 **Core Features**

- **Strict Layered Architecture** — Reusable engine core (`core/`) cleanly separated from game-specific code (`game/skyguard/`)
- **Data-Driven Design** — All gameplay, configs, and resources defined in JSON; zero hardcoded magic constants
- **Entity–Component–System (ECS)** — Performance-focused ECS with EnTT backend, designed for 100K+ entities
- **ID-Based Resource Management** — Type-safe resource IDs, centralized registry, automatic caching and validation
- **Resolution-Independent UI** — Anchor policies, scaling behaviors, and lock systems for adaptive layouts
- **Deterministic Simulation** — Fixed timestep, stable iteration order, seeded RNG for replay and multiplayer support
- **Production-Grade Tooling** — Structured logging, debug overlay, profiling hooks, and hot-reload infrastructure

---

## 🏛️ **Design Principles**

1. **Data-Driven First** — Behavior expressed through configs and blueprints, not inheritance hierarchies
2. **ECS Over OOP** — Components are pure data, systems are pure logic, no virtual gameplay classes
3. **Separation of Concerns** — Core stays game-agnostic and cross-platform; simulation ≠ rendering
4. **Performance Over Beauty** — Target: 60 FPS with hundreds of thousands of active entities
5. **Fail-Fast Validation** — Invalid configs, broken registries, and impossible invariants terminate immediately
6. **Incremental Complexity** — Start simple, add features gradually as profiling reveals bottlenecks

---

## 🧱 **Architecture Overview**

### ECS Core (`core::ecs`)
- **Entity** — Lightweight numeric ID
- **Component** — Plain data structs (Transform, Velocity, Sprite, etc.)
- **System** — Logic operating over entity views (Movement, Rendering, Input, Scaling, etc.)
- **World** — Thin façade over EnTT registry coordinating entities and systems

### Resource Layer (`core::resources`)
```
ResourceManager (façade)
    ↓
ResourcePaths (ID → Config registry from resources.json)
    ↓
ResourceHolder<T, ID> (per-type cache)
    ↓
*Resource wrappers (Texture, Font, SoundBuffer)
    ↓
SFML primitives
```

**Key Principle:** Resources accessed via strongly-typed IDs (`TextureID::PlayerShip`), never raw strings or SFML types in gameplay code.

### Configuration Pipeline
```
JSON files → FileLoader → JsonValidator → Typed Blueprints/Settings → Systems
```

All configs validated on load; failures are logged and terminate early. Engine settings and game configs are separate (`assets/core/` vs `assets/game/skyguard/`).

### Time & Main Loop
- `TimeService` — Fixed timestep updates, frame timing, FPS metrics
- `Game` — Composition root owning World, ResourceManager, and the main loop
- Deterministic simulation via stable `dt`, ordered system execution, and seeded RNG

---

## 📁 **Project Structure**

```text
SFML1/
├─ assets/
│  ├─ core/config/           # Engine configs (engine_settings.json, debug_overlay.json)
│  ├─ core/fonts/            # Shared engine fonts
│  └─ game/skyguard/
│     ├─ config/             # Game configs (player.json, resources.json)
│     ├─ images/             # Textures and sprites
│     └─ sounds/             # Audio assets
├─ include/
│  ├─ core/                  # Engine headers (ECS, resources, time, UI, logging)
│  ├─ game/skyguard/         # SkyGuard-specific headers
│  ├─ nlohmann/			     # nlohmann-json (vendored)
│  ├─ third_party/           # Thin wrappers over nlohmann-json (json_silent, json_fwd, LICENSE)
│  └─ pch.h                  # Precompiled header (shared across core/game)
├─ src/
│  ├─ core/                  # Engine implementations
│  └─ game/skyguard/         # Game implementations
├─ docs/architecture/        # Architecture documentation
├─ tools/                    # Static analysis and layering checks
└─ main_skyguard.cpp         # Entry point

```

**Full structure:** See [STRUCTURE.md](./STRUCTURE.md)

---

## 🧩 **Build Instructions**

### Prerequisites
- **C++20-capable compiler** (MSVC 19.29+, Clang 13+, GCC 11+)
- **SFML 3.0.2** ([Download](https://www.sfml-dev.org/download/sfml/3.0.2/))
- **Visual Studio 2026+** (Windows primary target)
- **nlohmann-json** (vendored, no install needed)

### Building with Visual Studio (Windows)

1. **Install SFML 3.0.2**
   ```bash
   # Extract to: C:\dev\SFML-3.0.2-64
   ```

2. **Clone the repository**
   ```bash
   git clone https://github.com/v-parjomenco/SFML1.git
   cd SFML1
   ```

3. **Open solution**
   - Open `SFML1.sln` in Visual Studio 2026+
   - Configure SFML include/lib paths in project properties
   - Select configuration: `Debug x64` or `Release x64`

4. **Build and run**
   - Entry point: `main_skyguard.cpp`
   - Assets are loaded from `SFML1/assets/`
   - Logs written to `SFML1/logs/`

### Cross-Platform Builds
CMake support for Linux/macOS is planned for Phase 2.

---

## ⚙️ **Development Tools & Code Style**

### Code Standards
- **C++20 modern style** — No legacy OOP hierarchies, embrace ECS and data-driven patterns
- **Indentation:** 4 spaces, no tabs
- **Braces:** Same line (`if (...) {`)
- **Naming:** `snake_case` files, `PascalCase` classes, `camelCase` members
- **Include order:** Corresponding header → STL → SFML → Project headers (sorted alphabetically per block)
- **Value initialization:** Always use `Type obj{};` for Config/Blueprint/Settings/Component types

### Static Analysis & Formatting

```bash
# Install LLVM tools (Windows)
winget install LLVM.LLVM

# Run clang-tidy
.\tools\run-clang-tidy.ps1

# Check include layering (core vs game separation)
.\tools\check_layering.ps1

# Format code (recommended: enable "format on save")
clang-format -i $(git ls-files *.h *.cpp)
```

---

## 🤝 **Contributing Guidelines**

1. **Follow ECS principles** — Components are data, systems are logic, no gameplay inheritance
2. **Keep core game-agnostic** — Engine code in `core/` must never depend on `game/`
3. **Validate all configs** — Use `JsonValidator` for all JSON parsing
4. **Log appropriately** — Use correct categories (`Engine`, `Config`, `Resources`, `Gameplay`, etc.)
5. **Value-initialize data types** — `PlayerBlueprint player{};` not `PlayerBlueprint player;`
6. **Document non-obvious code** — File headers in English, inline comments in Russian
7. **Test before committing** — Project must build and run after every change
8. **Avoid external dependencies** — Only add libraries when absolutely essential

---

## 🧰 **Quick Start for Developers**

### Adding a New Resource

1. **Add asset file**
   ```bash
   assets/game/skyguard/images/ship.png
   ```

2. **Register in resources.json**
   ```json
   {
     "textures": {
       "PlayerShip": "assets/game/skyguard/images/ship.png"
     }
   }
   ```

3. **Use in code**
   ```cpp
   auto& texture = resourceManager.getTexture(TextureID::PlayerShip);
   sprite.setTexture(texture.get());
   ```

### Creating a New ECS Component

1. **Define in `include/core/ecs/components/`**
   ```cpp
   // health_component.h
   struct HealthComponent {
       int current{100};
       int maximum{100};
   };
   ```

2. **Register with World**
   ```cpp
   world.registerComponent<HealthComponent>();
   ```

3. **Use in systems**
   ```cpp
   auto view = world.view<HealthComponent, TransformComponent>();
   for (auto entity : view) {
       auto& health = view.get<HealthComponent>(entity);
       // ... logic
   }
   ```

### Creating a New ECS System

1. **Implement in `include/core/ecs/systems/`**
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

2. **Register in Game initialization**
   ```cpp
   systemManager.addSystem<HealthRegenSystem>();
   ```

---

## 🧭 **Roadmap**

### Phase 1: Foundation (Current)
- [x] ECS framework with gradual migration to EnTT backend
- [x] Resource management with ID-based registry
- [x] Fixed timestep time service
- [x] Config-driven architecture
- [x] Debug overlay and structured logging
- [ ] Hot-reload for JSON configs
- [ ] Memory profiling and custom allocators
- [ ] Stress testing with 10K+ entities

### Phase 2: Engine Maturity (Year 1-2)
- [ ] CMake builds and cross-platform support (Linux/macOS)
- [ ] Asynchronous resource loading and streaming
- [ ] Spatial indexing (quad-tree) for large worlds
- [ ] Event bus and scriptable triggers
- [ ] Save/load system with versioning
- [ ] Multiple prototype games (Bomberman-like, platformer, etc.)

### Phase 3: 4X Grand Strategy (Year 3-5)
- [ ] Layered simulation (tactical/operational/strategic)
- [ ] Data-driven AI (Utility AI + GOAP/HTN + Behavior Trees)
- [ ] Deterministic replay and lock-step multiplayer
- [ ] Delta saves and chunked loading
- [ ] Paradox-style event system (scriptable triggers/effects)
- [ ] Mod support and content pipeline

---

## 📚 **Documentation**

- **[Architecture Reference](./docs/architecture/)** — Deep-dive into engine design
- **[STRUCTURE.md](./STRUCTURE.md)** — Detailed file layout
- **Code Comments** — Inline Russian comments explain intent and invariants

---

## 🪪 **License**

- **Engine code:** License TBD (to be formalized)
- **Third-party libraries:** nlohmann-json under MIT License — see [LICENSE.MIT](./SFML1/include/third_party/LICENSE.MIT)

---

## 🙏 **Credits**

See [CREDITS.md](./CREDITS.md) for acknowledgments.

---

**Built with passion for scalable, data-driven game architecture.**  
*Performance over beauty. Determinism over randomness. Code that lasts.*