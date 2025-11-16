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
├─ assets/              # Game assets (config, fonts, images, sounds)
├─ data/                # Engine definitions (resources.json)
├─ include/             # Header files
│  └─ core/             # Core engine
│     ├─ config/        # JSON config loading
│     ├─ ecs/           # Entity-Component-System
│     ├─ resources/     # Resource management
│     ├─ ui/            # UI behaviors (anchors, scaling)
│     └─ utils/         # Utilities (JSON, messaging)
├─ src/                 # Implementation files
└─ main.cpp             # Entry point
```

**Full structure:** See [STRUCTURE.md](./STRUCTURE.md)

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

## 🎯 **Project Vision**

**SkyGuard** is a modular game engine designed to support:
1. **Phase 1 (Current):** Aerial combat prototype
2. **Phase 2 (Year 1-2):** Casual games to test engine flexibility
3. **Phase 3 (Year 3-5):** Full 4X grand strategy game (Civ/EU5 scale)

---

## 🏛️ **Design Principles**

1. **Data-Driven First** - Gameplay defined in JSON, not hardcoded
2. **Separation of Concerns** - Simulation logic ≠ Rendering logic
3. **Paradox-Style Modding** - Inspired by EU4/CK3 event/trigger system
4. **Performance Over Beauty** - 60 FPS with 10K entities > fancy graphics
5. **Incremental Complexity** - Start simple, add features gradually

---

## 🪪 **License**
Distributed under the **MIT License** — see [LICENSE.MIT](./SFML1/include/third_party/LICENSE.MIT).
