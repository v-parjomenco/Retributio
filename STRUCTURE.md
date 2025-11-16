# **SkyGuard** *(name subject to change)*

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