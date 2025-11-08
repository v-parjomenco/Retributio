# **SkyGuard** *(name subject to change)*  

A modular 2D **C++17 + SFML 3.0.2** game engine powering an aerial combat indie game — and designed to scale up to full-scale 4X and simulation titles.

---

## 🚀 **Features**
- **Config-driven architecture** — gameplay, input, and scaling defined in JSON.  
- **Entity–Component–System Core** — clean data-driven structure for all logic.  
- **Unified Resource Management** — textures, fonts, and sounds handled safely with auto-unloading.  
- **UI Anchors and Scaling Policies** — adaptive layout on any resolution.  
- **Cross-platform** — consistent builds on Windows, Linux, and macOS.  

---

## 📁 **Project Structure**
```text
SFML1/
├─ assets/                     # Game assets
│  ├─ config/                  # JSON configs (e.g. player.json)
│  ├─ fonts/                   # Fonts (e.g. Wolgadeutsche.otf)
│  ├─ images/                  # Sprites and textures
│  └─ sounds/                  # Sound effects and music
│
├─ data/                       # Engine data and definitions
│  └─ definitions/             # Resource definitions (e.g. resources.json)
│
├─ include/                    # Header files (engine API)
│  ├─ core/                    # Core engine logic
│  │  ├─ config/               # Config keys and loader
│  │  ├─ ecs/                  # Entity–Component–System framework
│  │  │  ├─ components/        # Component definitions
│  │  │  ├─ systems/           # Game logic systems (render, movement, etc.)
│  │  │  └─ detail/            # Internal ECS mechanisms
│  │  ├─ resources/            # Resource management layer
│  │  │  ├─ holders/           # Generic resource holders (templates)
│  │  │  ├─ ids/               # Enum identifiers and string helpers
│  │  │  ├─ loader/            # Resource loading logic
│  │  │  ├─ paths/             # Resource path definitions
│  │  │  └─ types/             # Resource type wrappers (texture, font, sound)
│  │  ├─ ui/                   # UI behaviors, anchors, scaling & lock policies
│  │  └─ utils/                # Core utilities (JSON, messaging, etc.)
│  │     └─ json/              # JSON parsing & validation helpers
│  │
│  ├─ entities/                # Game entities (Player, NPCs, etc.)
│  ├─ graphics/                # Rendering utilities (future expansion)
│  ├─ third_party/             # External libraries (e.g. nlohmann/json)
│  └─ utils/                   # Global helper utilities
│
├─ src/                        # Source code (.cpp implementations)
│  ├─ core/                    # Core engine source
│  │  ├─ config/               # Config loader implementation
│  │  ├─ ecs/                  # ECS framework implementation
│  │  │  ├─ components/        # Component logic (if specialized)
│  │  │  ├─ systems/           # System logic (movement, rendering, etc.)
│  │  │  └─ detail/            # Internal ECS routines
│  │  ├─ resources/            # Resource manager, holders, loader, paths, types
│  │  │  ├─ holders/           # Template implementations for holders
│  │  │  ├─ ids/               # Resource ID utilities
│  │  │  ├─ loader/            # ResourceLoader implementations
│  │  │  ├─ paths/             # Path resolver logic
│  │  │  └─ types/             # Resource type behavior
│  │  ├─ ui/                   # UI systems (anchor, scaling, lock behavior)
│  │  └─ utils/                # Utility and JSON implementation
│  │     └─ json/              # JSON parsing & validation code
│  │
│  ├─ entities/                # Entity logic implementations
│  ├─ graphics/                # Graphics helpers and rendering classes
│  ├─ third_party/             # Stubs or wrappers for external code (rarely used)
│  └─ utils/                   # Standalone helper functions
│
├─ CREDITS.md                  # Contributors and acknowledgements
├─ README.md                   # Project documentation
├─ SFML1.sln                   # Visual Studio solution
└─ LICENSE.MIT                 # MIT License
```

---

## 🧩 **Build Instructions**

### Prerequisites
- **C++17** or later  
- **SFML 3.0.2** ([Download](https://www.sfml-dev.org/download/sfml/3.0.2/))  
- **Visual Studio 2022** *(recommended)* or any IDE with CMake support  

### Build (Visual Studio)
1. Clone the repository:
   ```bash
   git clone https://github.com/v-parjomenco/SFML1.git
   ```
2. Open `SFML1.sln`.  
3. Set SFML include & lib directories.  
4. Choose **Debug** or **Release**.  
5. Build and run.

---

## 🧱 **Architecture Overview**

The engine is designed around a **Data-Driven ECS** core:  
- **Entity** → lightweight ID, no behavior.  
- **Component** → data-only (transform, velocity, sprite, etc.).  
- **System** → operates on entities with specific components.

Typical systems:
- `MovementSystem` — updates entity motion.  
- `RenderSystem` — draws all visible sprites.  
- `ScalingSystem` — adjusts visuals on window resize.  
- `LockSystem` — enforces screen anchoring rules.  

Everything — from controls to world size — is configurable via JSON.

---

## ⚙️ **Coding Style & Tooling**

To ensure long-term maintainability and team scalability, *SkyGuard* enforces consistent coding and QA tools.

### Code Style
- **clang-format**: unified C++17 formatting  
- **clang-tidy**: static analysis for runtime safety and performance  
- **pre-commit hooks**: automatic code check before commits  

#### Recommended Setup
```bash
# Install LLVM tools (Windows)
winget install LLVM.LLVM

# Format all code
clang-format -i $(git ls-files *.h *.cpp)

# Static analysis
clang-tidy $(git ls-files *.cpp)
```

*(Optional: enable “Format on Save” in your IDE for consistency.)*

---

## 🤝 **Contributing Guidelines**

- Follow **ECS** and **data-driven** principles.  
- Keep all code C++17-compliant and portable.  
- Use **snake_case** for files, **PascalCase** for classes, and **camelCase** for members.  
- Document all public APIs and non-trivial functions.  
- Keep commits atomic and PRs focused.  
- Avoid adding external dependencies unless essential.  

---

## 🧰 **Developer Notes**

### Adding a New Resource
1. Add the resource file to `assets/` (e.g. `assets/images/ship.png`).
2. Register it in `data/definitions/resources.json`:
   ```json
   {
     "textures": {
       "PlayerShip": "assets/images/ship.png"
     }
   }
   ```
3. Load it in your code through the `ResourceManager`:
   ```cpp
   auto& tex = resourceManager.get<TextureID>(TextureID::PlayerShip);
   sprite.setTexture(tex);
   ```

### Creating a New ECS Component
1. Create a new header in `include/core/ecs/components/`:
   ```cpp
   struct HealthComponent {
       int current;
       int max;
   };
   ```
2. Register it in your ECS `World` or `Registry`.
3. Systems can now query entities that have this component.

### Creating a New ECS System
1. Add a new header in `include/core/ecs/systems/` and its implementation in `src/core/ecs/systems/`.
2. Derive from your `ISystem` base (if any) and override `update(World&, float dt)`.
3. Register the system in `World` initialization.

### JSON Integration
All configuration files are validated through `core::utils::json::JsonValidator`.  
Use `parseValue` helpers to safely extract data types.

---

## 🧭 **Roadmap**
- [ ] Asynchronous resource loading  
- [ ] Memory management (LRU / streaming)  
- [ ] Hot-reload JSON configs  
- [ ] ECS-based physics and AI  
- [ ] Widget/UI system  
- [ ] Modular game state management  

---

## 🪪 **License**
Distributed under the **MIT License** — see [LICENSE.MIT](./SFML1/include/third_party/LICENSE.MIT).
