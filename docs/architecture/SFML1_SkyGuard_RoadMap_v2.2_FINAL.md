# SFML1 / SkyGuard → 4X "Titan"
## ARCHITECTURE & ROADMAP v2.2 (FINAL)

**Last Updated:** 2026-01-04  
**Status:** Active Development - Phase 0/1

---

## 🎯 PROJECT VISION

### **SkyGuard (Short-term, 6 months)**
- **Genre:** Real-time arcade flight simulator
- **Target Audience:** Kids 6-13 (all genders welcome)
- **Style:** Bright graphics, real aircraft models, simplified 90s arcade mechanics
- **Educational:** Accessible technical info about real aircraft
- **Goal:** Complete, sellable product validating core architecture
- **Revenue Target:** $10K-50K Steam sales → fund 4X development

### **Titan (Long-term, 5 years)**
- **Genre:** Scientific 4X Grand Strategy
- **Scale:** 1M entities, 100K unique textures, multi-planetary (Earth + Mars/Venus/etc.)
- **AI:** Neural AI integration (Year 2+), scientific-grade decision-making
- **Style:** Tavern-pirate aesthetic (Sid Meier's Colonization / Port Royale 2 inspiration)
- **Priority:** Single-player excellence FIRST, multiplayer architecture built-in
- **Goal:** Deep historical simulation rivaling Paradox (EU4/HOI4) but larger scale

---

## 🧱 DESIGN GATES (NON-NEGOTIABLE)

### **Core Layer**
- ✅ Game-agnostic (no genre-specific rules)
- ✅ No arcade physics in core
- ✅ No UI logic for specific games
- ✅ Zero RTTI in hot paths
- ✅ Zero-overhead abstractions only

### **Game Layer**
- ✅ SkyGuard = real-time arcade (separate from core)
- ✅ Titan 4X = turn-based simulation (separate from core)
- ✅ Shared core, different contracts

### **Dev-Only**
- ✅ Stress/soak/harness scenes ONLY under `SFML1_PROFILE`
- ✅ Never ship in Release builds
- ✅ Validate architecture risks separately from gameplay

---

## 🧪 BUILD MODES

| Mode    | Purpose                                  |
|---------|------------------------------------------|
| Release | Game builds (Steam releases)             |
| Debug   | Functional debugging, assertions enabled |
| Profile | Stress tests, determinism CI, memory budgets, Tracy profiling |

---

## 🧭 ROADMAP

---

## **PHASE 0 — Project Foundation** (Week 0)

**Goal:** Clean repository structure, build system, zero-overhead guarantees.

### Deliverables:
- [ ] Repository layout (`core/`, `game/skyguard/`, `game/titan/`, `assets/`)
- [ ] Build modes (Release, Debug, Profile)
- [ ] **CMake Build System** (cross-platform, vcpkg integration)
- [ ] Logging policy (`Engine`, `ECS`, `Resources`, `UI`, `Gameplay`, `Performance` categories)
- [ ] Compile-time gates (`SFML1_PROFILE`, `NDEBUG`)
- [ ] Zero-overhead guarantee validated (Release builds have no debug bloat)

---

## **PHASE 1 — Core & Determinism Foundation** (Weeks 1-4)

**Goal:** ECS architecture + determinism core + SkyGuard prototype in parallel.

---

### **Week 1: ECS & System Contracts**

**Deliverables:**
- [ ] **ECS Integration:** EnTT 3.16.0
- [ ] **System Metadata:** Component access declarations
  ```cpp
  using ComponentID = entt::id_type;  // NO std::type_index (no RTTI)
  
  class ISystem {
      virtual std::span<const ComponentID> getReadComponents() const noexcept = 0;
      virtual std::span<const ComponentID> getWriteComponents() const noexcept = 0;
  };
  ```
- [ ] **Stable Iteration Guarantees:** EnTT view semantics documented
- [ ] **Tag Components for Player:**
  ```cpp
  struct LocalPlayerTag {};   // Camera follows, input routing
  struct Player1Tag {};       // 1v1 mode: first player
  struct Player2Tag {};       // 1v1 mode: second player
  ```

**Why:** Multiplayer-ready architecture from day 1, no tech debt.

---

### **Week 1-2: SkyGuard Prototype (Minimal, Dirty)**

**Goal:** Playable prototype ASAP (functionality over polish).

**Deliverables:**
- [ ] Window + render loop (SFML 3.0.2)
- [ ] ECS-driven entities (player plane, basic sprites)
- [ ] Player input (keyboard/gamepad)
- [ ] Flying aircraft (placeholder movement)
- [ ] Collision stub (AABB bounds, no actual collision yet)

**Success:** Game launches, plane responds to input, renders @ 240 FPS empty scene.

---

### **Week 2: Determinism Foundation (EXPLICIT MILESTONE)**

**Goal:** Lock down determinism for replay/multiplayer/saves.

**Deliverables:**
- [ ] **Compiler Flags:**
  - MSVC: `/fp:precise` (NO `/fp:fast`)
  - GCC/Clang: `-ffloat-store` (NO `-ffast-math`)
- [ ] **Centralized RNG:**
  ```cpp
  class DeterministicRNG {
      std::mt19937 mEngine{42};  // Serializable state
      
      float nextFloat(float min, float max);
      int nextInt(int min, int max);
      void setSeed(std::uint64_t seed);
      std::uint64_t getState() const;
  };
  ```
- [ ] **Physics Separation:**
  - SkyGuard arcade physics → `float` allowed (not deterministic, ok for real-time)
  - Titan simulation core → `fixed-point` or deterministic math libraries (future)
  - **IMPORTANT:** Don't implement fixed-point prematurely. ONLY if real desync issues appear in testing (Year 2+). Float determinism (`/fp:precise`) sufficient for initial implementation.
- [ ] **Tick-Based Time:**
  ```cpp
  class TimeService {
      std::uint64_t mTickNumber = 0;
      float mFixedDt = 1.f / 60.f;
      
      void tick();
      std::uint64_t getCurrentTick() const { return mTickNumber; }
  };
  ```

**Why:** Foundation for saves, replay, multiplayer. No "maybe it works" - determinism is HARD requirement.

---

### **Week 2: Phase-Based Scheduler**

**Goal:** Deterministic system ordering (NO modulo hacks).

**Deliverables:**
- [ ] **Update Phases:**
  ```cpp
  enum class UpdatePhase : std::uint8_t {
      Input,       // Always first (keyboard, gamepad, network)
      Simulation,  // Game logic (movement, collision)
      AI,          // AI decisions (4X future)
      Resolution,  // Combat, economy (4X future)
      Render       // Always last (gathering sprites, no draw in update)
  };
  
  class ISystem {
      virtual UpdatePhase getPhase() const = 0;
      virtual std::uint32_t getFrequency() const { return 1; }  // Updates per tick
  };
  ```
- [ ] **SystemManager Execution:**
  - Sort systems by phase
  - Within phase: check `(mTickNumber % frequency) == 0`
  - Deterministic ordering ALWAYS

**Why:** Avoid "AI runs before economy sometimes, after other times" bugs.

---

### **Week 3: Determinism Test Suite (CI Integration)**

**Goal:** Automated determinism validation (CI fails on mismatch).

**Deliverables:**
- [ ] **Headless Simulation Runner:**
  ```cpp
  #if defined(SFML1_PROFILE)
  std::uint64_t runHeadlessSimulation(std::uint64_t seed, int ticks) {
      DeterministicRNG rng(seed);
      World world;
      // ... spawn entities, run systems
      for (int i = 0; i < ticks; ++i) {
          world.update(fixedDt);
      }
      return computeChecksum(world);  // Hash all component data
  }
  #endif
  ```
- [ ] **Cross-Platform Tests:**
  ```cpp
  TEST(Determinism, CrossPlatformReplay) {
      std::uint64_t hash1 = runHeadlessSimulation(42, 1000);
      std::uint64_t hash2 = runHeadlessSimulation(42, 1000);
      ASSERT_EQ(hash1, hash2);  // EXACT match or CI RED
  }
  ```
- [ ] **CI Integration:** GitHub Actions runs determinism tests on every commit

**Why:** Catch desync bugs EARLY. Non-determinism is silent killer for multiplayer/replay.

---

### **Week 3-4: Profiling Infrastructure (Tracy Integration)**

**Goal:** Measure performance BEFORE optimizing.

**Deliverables:**
- [ ] **Tracy Profiler Integration:**
  ```cpp
  #if defined(SFML1_PROFILE)
      #include <tracy/Tracy.hpp>
      #define PROFILE_SCOPE(name) ZoneScopedN(name)
      #define PROFILE_FRAME() FrameMark
  #else
      #define PROFILE_SCOPE(name) do {} while(0)
      #define PROFILE_FRAME() do {} while(0)
  #endif
  ```
- [ ] **Instrumented Systems:**
  - `MovementSystem::update()` → track µs per 100K entities
  - `RenderSystem::gather()` → track sort/cull time
  - `SpatialIndex::query()` → track query cost
- [ ] **Stress Test on Virtual Data:**
  ```cpp
  #if defined(SFML1_PROFILE)
  void stressTestVirtual() {
      // 100K entities, 1-2 real textures, rest dummy handles
      for (int i = 0; i < 100'000; ++i) {
          auto e = world.createEntity();
          world.addComponent<TransformComponent>(e, randomPos());
          world.addComponent<VelocityComponent>(e, randomVel());
          world.addComponent<SpriteComponent>(e, dummyTextureHandle);
      }
      PROFILE_SCOPE("Stress_100K");
      world.update(fixedDt);
  }
  #endif
  ```
- [ ] **CSV Export:** CI tracks performance regression (FPS, system timings)

**Why:** "Profile-guided development" - know where to optimize, not guess.

---

### **Week 4: 4X Simulation Harness (DEV-ONLY, HEADLESS)**

**Goal:** Validate 4X-scale architecture risks WITHOUT building 4X game.

**Deliverables:**
- [ ] **Headless Harness:**
  ```cpp
  #if defined(SFML1_PROFILE)
  void run4XSimulationHarness() {
      // 100K-500K entities (provinces, units, cities)
      // Turn-based cadence (1 turn = 60 ticks)
      // Save/load every 10 turns
      // Checksum validation
      // Spatial queries (10K range queries/turn)
      // AI scheduling simulation (1000 decisions/turn)
  }
  #endif
  ```
- [ ] **Validated Risks:**
  - ✅ Spatial index performance under 100K-500K entities
  - ✅ Save/load 500K entities (EnTT snapshot)
  - ✅ Memory pressure (target: <4GB for 500K entities)
  - ✅ AI scheduling (can we budget 1000 AI decisions/turn?)

**Why:** SkyGuard (arcade) doesn't stress 4X risks. Harness fills gap.

---

### **Week 4: Hot-Reload Infrastructure**

**Goal:** Rapid iteration on game parameters (NO restart needed).

**Deliverables:**
- [ ] **Config File Watcher:**
  ```cpp
  class ConfigWatcher {
      void watch(const std::filesystem::path& path, 
                 std::function<void()> onChanged);
  };
  
  // SkyGuard example:
  mConfigWatcher.watch("assets/game/skyguard/config/player.json",
      []() { reloadPlayerStats(); });
  
  // Titan example (future):
  mConfigWatcher.watch("assets/game/titan/config/provinces.json",
      []() { reloadProvinceData(); });
  ```
- [ ] **Universal Design:** Works for all games (SkyGuard + Titan)

**Why:** Tune arcade physics without restart = 10x faster iteration.

---

## **PHASE 2 — Resources & Rendering Core** (Months 2-3)

**Goal:** Resource management, deterministic loading, render hot-path audit.

---

### **Deliverables:**
- [ ] **Resource Registries:**
  - Type A loaders: hard-fail (textures, fonts - game breaks without)
  - Type B loaders: soft-fail (sounds - game continues)
  - Type C loaders: optional (mods, DLC)
- [ ] **Zero-Throw JSON Parsing:**
  ```cpp
  // noexcept paths for hot-reloadable configs
  std::optional<Config> tryLoadConfig(const std::filesystem::path& path) noexcept;
  ```
- [ ] **Texture Handle System:**
  - ID-based (entt::resource_handle or custom)
  - Deterministic loading (std::sort by enum ID)
- [ ] **Render Hot-Path Audit:**
  - NO per-entity validation in Release
  - Assertions ONLY in Debug/Profile
  - Cache-friendly iteration (EnTT packed views)

**Why:** Performance-first resource management. Release builds FAST.

---

## **PHASE 3 — SkyGuard MVP (Complete Game)** (Months 3-6)

**Goal:** Finished, sellable product. Steam release.

---

### **Month 3: Core Gameplay Systems**

**Deliverables:**
- [ ] **Real-Time Game Loop:** Fixed timestep (60 FPS logic, 240 FPS render target)
- [ ] **Arcade Physics:** Simple movement, inertia, bounds checking
- [ ] **Projectile System:**
  ```cpp
  struct ProjectileComponent {
      float lifetime;
      float damage;
      Entity owner;
  };
  
  class ProjectileSystem {
      void update(World& world, float dt) {
          auto view = world.view<ProjectileComponent, TransformComponent, VelocityComponent>();
          for (auto [entity, proj, transform, vel] : view.each()) {
              proj.lifetime -= dt;
              if (proj.lifetime <= 0.f) {
                  world.destroyEntity(entity);
              }
          }
      }
  };
  ```
- [ ] **Collision Detection:** AABB or circle (simple, fast)
- [ ] **Enemy AI:** 3-5 enemy types (chase, patrol, shoot behaviors)
- [ ] **Mission System:** Data-driven missions (JSON: spawn waves, objectives, win/lose)

---

### **Month 3-4: Audio System (SEPARATE from Polish)**

**Why:** Audio is MISSING, not polishing. Separate milestone.

**Deliverables:**
- [ ] **Audio Architecture:**
  ```cpp
  class AudioSystem {
      void playEngineSound(Entity plane, float throttle) {
          // Spatial 2D audio, pitch varies with speed
      }
      
      void playExplosion(sf::Vector2f position) {
          // One-shot positional sound
      }
      
      void playMusic(MusicID id, bool loop);
  };
  ```
- [ ] **SkyGuard Sounds:**
  - Engine sounds (pitch modulation with speed)
  - Weapon fire (machine guns, missiles)
  - Explosions (enemy destroyed, player hit)
  - Menu music
- [ ] **Type B Loader:** Soft-fail if sound files missing (game continues)

**Success:** Immersive audio feedback for kids.

---

### **Month 3-4: Particle System (SEPARATE from VFX)**

**Why:** Particle System = gameplay VFX engine, not just "polish".

**Deliverables:**
- [ ] **Particle Architecture:**
  ```cpp
  struct ParticleComponent {
      float lifetime;
      sf::Color startColor;
      sf::Color endColor;
      sf::Vector2f velocity;
      float fadeRate;
  };
  
  class ParticleSystem {
      void spawnExplosion(sf::Vector2f pos, int count) {
          for (int i = 0; i < count; ++i) {
              auto e = world.createEntity();
              world.addComponent<ParticleComponent>(e, {...});
              world.addComponent<TransformComponent>(e, pos);
              world.addComponent<VelocityComponent>(e, randomDir());
          }
      }
  };
  ```
- [ ] **Performance Target:** 10,000 particles @ 240 FPS
  - Each particle = tiny quad (4 vertices) or point sprite
  - Batch rendering (single draw call per particle type)
- [ ] **Effects:**
  - Explosions (50-100 particles each)
  - Smoke trails (continuous spawn behind plane)
  - Muzzle flashes (burst on weapon fire)

**Success:** Juicy visual feedback, 10K particles alive simultaneously @ 240 FPS.

---

### **Month 4: 1v1 Multiplayer (SkyGuard Dogfight Mode)**

**Goal:** Play with friend feature (bonus mode, NOT core).

**Tech Stack:**
- **Steamworks P2P:** Peer-to-peer (NAT traversal free via Steam Relay)
- **State Synchronization:** Send position/rotation/velocity 20 times/sec + interpolation
  - Why NOT lockstep: Real-time arcade = state sync simpler
  - Lockstep reserved for Titan 4X (turn-based)

**Deliverables:**
- [ ] **Network State Packet:**
  ```cpp
  struct NetworkState {
      sf::Vector2f position;
      sf::Vector2f velocity;
      float rotation;
      std::uint8_t health;
      std::uint8_t ammo;
  };
  ```
- [ ] **Client Interpolation:**
  ```cpp
  class NetworkSystem {
      void sendState();      // Host: 20 Hz
      void receiveState();   // Client: interpolate between states
  };
  ```
- [ ] **Host/Client Architecture:** Simple lobby (no matchmaking)
- [ ] **Steam Invite System:** "Play with Friend" button

**Success:** 1v1 dogfight mode playable, <100ms latency feel.

---

### **Month 4-5: Progression & Retention**

**Deliverables:**
- [ ] **Hangar System:** Aircraft selection screen
- [ ] **Unlock System:** JSON-driven progression (score → unlock new planes)
- [ ] **Local Persistence:** Simple file I/O (NOT EnTT snapshot, just player progress)
  ```json
  {
    "totalScore": 15000,
    "unlockedPlanes": ["F-16", "SU-27", "MiG-29"],
    "currentPlane": "SU-27"
  }
  ```
- [ ] **Leaderboards:** Local high scores (Steam leaderboards optional)

---

### **Month 5: UI/UX Polish**

**Deliverables:**
- [ ] **Main Menu:** Start, Options, Quit
- [ ] **Aircraft Selection:** Visual hangar with 5-7 planes
- [ ] **HUD:** Speed, altitude, ammo, health (kid-friendly design)
- [ ] **Results Screen:** Score, stats, retry/quit buttons
- [ ] **Simple Animations:** Menu transitions, button hovers

---

### **Month 5-6: Steam Integration**

**Goal:** Steam features for visibility and retention.

**Deliverables:**
- [ ] **Steamworks SDK Integration:**
- [ ] **Achievements:**
  - "First Kill" (destroy 1 enemy)
  - "Ace Pilot" (10 kills in one mission)
  - "Untouchable" (complete mission without damage)
  - 10-15 total achievements
- [ ] **Cloud Saves:** Player progression JSON synced
- [ ] **Rich Presence:** "Flying SU-57 in Dogfight Mode"
- [ ] **Steam Input API:** Gamepad support (optional, keyboard/mouse primary)

---

### **Month 6: Visual & Gameplay Polish**

**Deliverables:**
- [ ] **VFX Juice:**
  - Screen shake (explosions, hits)
  - Hit stop (brief pause on impact)
  - Flash effects (damage feedback)
  - Camera follow smoothing
- [ ] **Content Expansion:**
  - 5-7 aircraft models (different stats: speed, armor, weapons)
  - 3-5 maps (different environments: ocean, mountains, desert)
  - Tutorial mission (basic controls)
- [ ] **Settings Menu:**
  - Graphics (resolution, vsync, effects quality)
  - Audio (master, music, SFX volumes)
  - Controls (rebindable keys, gamepad sensitivity)
- [ ] **Optimization & QA:**
  - Tracy profiling → identify bottlenecks
  - Valgrind → verify no memory leaks
  - Wide compatibility testing (integrated graphics, wide screens)

**Success Metrics:**
- [ ] 240 FPS on mid-range hardware (GTX 1060 equivalent)
- [ ] 60 FPS on integrated graphics (Intel UHD)
- [ ] No crashes in 2-hour playtesting sessions
- [ ] <100ms latency feel in 1v1 multiplayer

---

## 🚀 **RELEASE: SkyGuard on Steam (Month 6)**

**Price:** $5-10  
**Revenue Goal:** $10K-50K → fund Titan 4X development  
**Platform:** Steam (Windows initially, Linux/Mac via CMake builds)

---

## **PHASE 4 — Titan 4X Foundation** (Months 7-12)

**Goal:** Architecture for 1M entities. MVP = 10-20 civs, infrastructure = planetary scale.

---

### **Month 7: Multi-Planet Map Infrastructure**

**Goal:** Design for universe scale from day 1.

**Deliverables:**
- [ ] **Planetary Map System:**
  ```cpp
  enum class PlanetID : std::uint32_t {
      Earth = 1,
      Mars = 2,      // Future
      Venus = 3,     // Future
      // Extensible via JSON
  };
  
  class PlanetaryMap {
      std::unordered_map<PlanetID, ChunkedTileMap> mPlanets;
      
      void addPlanet(PlanetID id, const MapConfig& config) {
          mPlanets[id] = ChunkedTileMap::generate(config);
      }
  };
  ```
- [ ] **Earth Map (Giant):**
  - 10,000 provinces (Voronoi or grid-based generation)
  - Wasteland regions: Graphically exist, minimal data, non-playable (like EU4)
  - Playable core: ~2000 provinces with full simulation data
- [ ] **Multi-Planet Ready:**
  - Infrastructure allows: `mPlanetaryMap.addPlanet(PlanetID::Mars, marsConfig);`
  - Mars/Venus content NOT created yet, but API ready

**Why:** Build foundation right. Add planets incrementally.

---

### **Month 7-8: Texture System (Multi-Atlas PRIMARY, Arrays OPTIONAL)**

**Goal:** Support 100K unique textures (50K minimum).

**Architecture Decision:**
- **PRIMARY:** Multi-atlas system (proven, reliable)
- **OPTIONAL:** Texture arrays (if hardware supports, fallback to multi-atlas)

**Deliverables:**
- [ ] **Multi-Atlas System:**
  ```cpp
  class MultiAtlasManager {
      enum class AtlasCategory : std::uint8_t {
          Buildings,  // 16K textures (64x64 icons)
          Units,      // 32K textures (64x64 unit sprites)
          Techs,      // 16K textures (128x128 tech icons)
          Terrain,    // 16K textures (terrain tiles)
          Leaders,    // 8K textures (leader portraits)
          UI,         // 8K textures (buttons, borders, etc.)
      };
      
      // Each atlas: 8192x8192 (128x128 grid of 64x64 tiles = 16K textures)
      std::unordered_map<AtlasCategory, sf::Texture> mAtlases;
      
      TextureRect registerTexture(AtlasCategory cat, const sf::Image& img);
  };
  ```
- [ ] **Atlas Capacity:**
  - Buildings: 16K (128x128 grid @ 64x64 tiles)
  - Units: 32K (needs 2 atlases OR 128x256 grid @ 64x64 tiles)
  - Techs: 16K
  - Terrain: 16K
  - Leaders: 8K
  - UI: 8K
  - **Total:** 4-8 atlases = 96K-128K textures
- [ ] **Batching Strategy:**
  - Draw calls grouped by atlas category
  - ~4-8 draw calls per frame (one per category)
  - Acceptable for 100K sprites
- [ ] **OPTIONAL: Texture Array Backend:**
  ```cpp
  #ifdef SFML1_USE_TEXTURE_ARRAYS
  class TextureArrayManager {
      GLuint mArrayTexture;  // OpenGL Texture Array (requires custom shader)
      std::uint16_t registerTexture(const sf::Image& img);
  };
  #endif
  ```
  - ONLY if hardware supports 4K+ layers
  - Custom OpenGL shader (fragment shader samples from array layer)
  - **CRITICAL:** ALWAYS fallback to multi-atlas (especially Intel integrated GPUs)
  - Test on Intel UHD 630 before relying on arrays

**Success:**
- [ ] Multi-atlas system handles 100K textures
- [ ] Batching by category = 4-8 draw calls
- [ ] Texture array optional enhancement (not required)

**Why:** Multi-atlas proven (Civilization III uses similar). Texture arrays experimental.

---

### **Month 8: Binary Serialization System**

**Goal:** Fast save/load for 1M entities.

**Deliverables:**
- [ ] **EnTT Snapshot Integration:**
  ```cpp
  class SaveSystem {
      void save(const World& world, const std::filesystem::path& path) {
          std::ofstream file(path, std::ios::binary);
          
          entt::snapshot snapshot{world.registry()};
          snapshot.entities(file)
                  .component<TransformComponent, 
                             ProvinceStateComponent, 
                             UnitStackComponent,
                             /*...all components*/>(file);
      }
      
      void load(World& world, const std::filesystem::path& path) {
          std::ifstream file(path, std::ios::binary);
          
          entt::snapshot_loader loader{world.registry()};
          loader.entities(file)
                .component<TransformComponent, /*...*/>(file);
      }
  };
  ```
- [ ] **Component Versioning:**
  ```cpp
  struct ProvinceStateComponent {
      static constexpr std::uint32_t kVersion = 3;
      
      // v1 fields
      std::uint32_t owner;
      std::uint32_t population;
      
      // v2 fields (added patch 1.5)
      float development = 1.0f;  // Default for migration
      
      // v3 fields (added patch 2.0)
      std::uint32_t tradePower = 0;
  };
  
  // Migration functions
  void migrate_v1_to_v2(ProvinceStateComponent& comp);
  void migrate_v2_to_v3(ProvinceStateComponent& comp);
  ```
- [ ] **Compression:** zlib for text-heavy data (province history strings)
- [ ] **Backward Compatibility:** N-1, N-2 version support (2 patches back)

**Success:**
- [ ] 1M entities save/load in <10 seconds
- [ ] Patch updates don't break saves

---

### **Month 9: Stable Entity ID System**

**Goal:** Persistent entity references for saves/network.

**Deliverables:**
- [ ] **Dense Vector Mapping:**
  ```cpp
  class StableIDMapper {
      std::vector<std::uint64_t> mEntityToStable;  // indexed by entt::to_integral(entity)
      std::unordered_map<std::uint64_t, entt::entity> mStableToEntity;  // reverse lookup (rare)
      std::uint64_t mNextID = 1;
      
      std::uint64_t getStableID(entt::entity e) {
          auto idx = entt::to_integral(e);
          if (idx >= mEntityToStable.size()) {
              mEntityToStable.resize(idx + 1, 0);
          }
          if (mEntityToStable[idx] == 0) {
              mEntityToStable[idx] = mNextID++;
              mStableToEntity[mEntityToStable[idx]] = e;
          }
          return mEntityToStable[idx];
      }
      
      entt::entity getEntity(std::uint64_t stableID) {
          return mStableToEntity[stableID];
      }
  };
  ```
- [ ] **Contract:** NEVER serialize `entt::entity` directly (generation counter reused)
- [ ] **Usage:** Save files, network packets use StableID only
- [ ] **Future Optimization (Year 2+):** If reverse lookup becomes bottleneck, replace `unordered_map` with `robin_hood::unordered_map` or `absl::flat_hash_map` (not urgent now)

**Why:** entt::entity = runtime only. Saves need persistent IDs.

---

### **Month 9-10: Data Pipeline & Modding Foundation**

**Goal:** Data-driven design + early modding infrastructure.

**Deliverables:**
- [ ] **Data Schemas (JSON Contracts):**
  - Province definitions (`id`, `name`, `terrain`, `resources`, `adjacency`)
  - Unit types (`id`, `name`, `stats`, `cost`, `requirements`)
  - Tech tree (`id`, `name`, `effects`, `prerequisites`, `cost`)
  - Building types, resource types, etc.
- [ ] **Simple Data Editor (Dev-Only):**
  - Ugly but functional (CSV/JSON editor)
  - NO fancy UI needed (Civ III editor inspiration for LATER)
- [ ] **Asset Binding:**
  - TextureID → province/unit/tech graphics
  - SoundID → events, battles, UI feedback
- [ ] **Mod Loading:**
  ```
  assets/core/        # Base game data
  assets/game/titan/  # Titan-specific data
  mods/               # User mods (override core)
  ```
- [ ] **Basic Map Tools:**
  - Import/export province data (JSON ↔ internal format)
  - Voronoi/grid generator (procedural Earth map)

**Why:** Data-driven = moddability foundation. Full scripting (Lua) later (v1.0 post-release).

---

### **Month 10-11: Heuristic AI Foundation**

**Goal:** Working AI FIRST, neural AI LATER (Year 2+).

**Deliverables:**
- [ ] **AI Architecture:**
  ```cpp
  struct AIDecisionComponent {
      std::variant<
          HeuristicDecision,    // NOW: utility-based
          NeuralDecision        // LATER: ONNX inference (optional)
      > decision;
  };
  
  class ProvinceAISystem {
      void evaluateExpansion(World& world, Entity province) {
          // Heuristic: military strength, economy, neighbors
          float score = calculateUtility(world, province);
          
          if (score > threshold) {
              world.addComponent<ExpansionGoalComponent>(province, target);
          }
      }
  };
  ```
- [ ] **Budgeted Execution:**
  - AI systems update at lower frequency (e.g., 1 Hz vs 60 Hz for physics)
  - Time-slicing: 1000 AI decisions spread across 10 ticks
- [ ] **Deterministic Behavior:** Same inputs → same decisions (for replay)
- [ ] **Serializable AI State:** Can save/load mid-decision

**Why:** Heuristic AI = 80% of quality. Neural AI = experimental layer on top (Year 2+).

---

### **Month 12: Headless Server Build**

**Goal:** Dedicated server foundation (NOT full multiplayer yet).

**Deliverables:**
- [ ] **Headless Build:**
  - NO rendering (no SFML window)
  - NO audio
  - Simulation only
- [ ] **Use Cases:**
  - AI stress tests (10K provinces, 1000 AI civs)
  - Simulation soak tests (1000 turns, verify no crashes)
  - CI performance regression (track FPS/turn time)
- [ ] **Future Multiplayer Base:**
  - Infrastructure ready for authoritative server (Year 3+)

**Why:** Test 4X scale without graphics overhead. Foundation for dedicated servers.

---

## **PHASE 5 — Titan 4X Core Gameplay** (Months 13-24)

**Goal:** Implement 4X mechanics. MVP = 10-20 civs, 10K provinces, 5K units.

---

### **Month 13-14: Province & Civilization Systems**

**Deliverables:**
- [ ] **Province System:**
  - ProvinceComponent (ownership, population, resources, loyalty)
  - ProvinceStateComponent (production, development, culture)
  - Adjacency graph, province borders
  - Fog of war
- [ ] **Civilization System:**
  - CivilizationComponent (government, ideology, treasury, technology)
  - Diplomacy relations (war, peace, alliance, trade agreement)
  - Tech tree (JSON-driven, 100+ technologies)
- [ ] **UI Integration:**
  - Province info panel (population, production, owner)
  - Diplomacy screen (relations, treaties)
  - Tech tree screen (interactive graph)

**Target:** 20 playable civs, 10K provinces (Earth map).

---

### **Month 15-16: Unit & Combat Systems**

**Deliverables:**
- [ ] **Unit System:**
  - UnitStackComponent (army composition: infantry, cavalry, artillery)
  - Movement (pathfinding A*, supply lines)
  - Combat resolution (deterministic, turn-based dice rolls)
- [ ] **Combat Mechanics:**
  - Battle calculator (attack/defense modifiers)
  - Casualties, retreats, sieges
  - Naval combat (optional, future expansion)
- [ ] **UI:**
  - Unit selection, movement commands
  - Battle summary screen

**Target:** 5K units total (500 visible on screen).

---

### **Month 16-17: Economy System**

**Deliverables:**
- [ ] **Economy Mechanics:**
  - Resource production (per province: food, gold, production, science)
  - Trade routes (internal + international)
  - Building construction (farms, markets, barracks, etc.)
  - Population growth (food-based)
- [ ] **UI:**
  - Economic overview (income/expenses)
  - Building queue
  - Trade route visualization

**Target:** Multi-resource economy (4-6 resource types minimum).

---

### **Month 17-18: Turn Manager & AI Time-Slicing**

**Deliverables:**
- [ ] **Turn Manager:**
  ```cpp
  void processTurn() {
      // 1. Player input phase
      applyPlayerActions();
      
      // 2. AI decision phase (parallel, time-sliced, 5-10 sec budget)
      mAIScheduler.processAllAI(mWorld);
      
      // 3. Resolution phase (deterministic, sequential)
      mCombatSystem.resolveAll(mWorld);
      mEconomySystem.tick(mWorld);
      mDiplomacySystem.tick(mWorld);
      
      // 4. Event triggers
      mEventSystem.checkTriggers(mWorld);
      
      // 5. Cleanup
      mWorld.destroyEntities(mPendingDestroys);
      
      ++mTurnNumber;
  }
  ```
- [ ] **AI Time-Slicing:**
  ```cpp
  class AIScheduler {
      void processAllAI(World& world) {
          const int aiPerBatch = 5;  // 5 civs at a time
          for (int batch = 0; batch < 4; ++batch) {
              processAIBatch(world, batch * aiPerBatch, aiPerBatch);
              // UI remains responsive
          }
      }
  };
  ```

**Success:** Turn processing <10 seconds for 20 AI civs.

---

### **Month 15-18: Network Readiness (4X Foundation, NOT Multiplayer)**

**Goal:** Architecture for turn-based multiplayer (implementation Year 3+).

**Deliverables:**
- [ ] **Deterministic Command Serialization:**
  ```cpp
  struct PlayerCommand {
      CommandType type;  // MoveUnit, BuildBuilding, DeclareWar, etc.
      std::uint64_t targetEntityStableID;
      std::vector<std::uint8_t> payload;
  };
  ```
- [ ] **Input Log:** Record all player commands (for replay)
- [ ] **Checksum Sync:**
  ```cpp
  std::uint64_t computeTurnChecksum(const World& world) {
      // Hash all deterministic game state
  }
  ```
- [ ] **Replay / Rollback Groundwork:**
  - Save state every N turns
  - Rollback on desync detection
- [ ] **NO UI, NO Matchmaking** (future)

**Why:** Lockstep deterministic multiplayer needs this foundation. Implementation later.

---

### **Month 19-20: Event System (Paradox-Style)**

**Deliverables:**
- [ ] **Event Definitions (JSON):**
  ```json
  {
    "eventID": "plague_outbreak",
    "trigger": {
      "provincePopulation": ">10000",
      "provinceHealth": "<50"
    },
    "effects": [
      {"type": "modifyPopulation", "value": -2000},
      {"type": "modifyLoyalty", "value": -10}
    ],
    "options": [
      {"text": "Quarantine (costs 100 gold)", "cost": 100, "effect": "stopSpread"},
      {"text": "Do nothing", "effect": "none"}
    ]
  }
  ```
- [ ] **Event Interpreter Over ECS:**
  - Trigger evaluation (check province components)
  - Effect application (modify components)
  - Event history timeline
- [ ] **UI:**
  - Event popup (historical flavor text, options)
  - Event log (past events, filter by type)

**Note:** Full Lua/scripting engine in v1.0 post-release. JSON events = foundation.

---

### **Month 21-22: Production UI (Tavern-Pirate Aesthetic)**

**Goal:** Beautiful, stylized UI (NOT ImGui, NOT office software).

**Architecture:**
- **ECS UI FOR:**
  - Scene graph (positioning, z-order, visibility)
  - Anchoring (resize handling)
  - Simple buttons/panels (SkyGuard-style)
- **NON-ECS (Custom Lightweight Layer) FOR:**
  - Virtualized lists (10K+ rows)
  - Tables (province lists, unit rosters)
  - Text editors (event text, diplomacy messages)
  - Heavy scroll/clip logic

**Deliverables:**
- [ ] **Diplomacy Screen:**
  - Nation portraits, relations matrix
  - Treaty negotiation UI (tables)
- [ ] **Tech Tree:**
  - Interactive graph (nodes = techs, edges = prerequisites)
  - Visual progress indicators
- [ ] **Province Info:**
  - Detailed stats (scrollable lists)
  - Building queue, production breakdown
- [ ] **Map Overlays:**
  - Political (civ borders, colors)
  - Terrain (mountains, forests, water)
  - Economic (resource distribution)
  - Diplomatic (allies, enemies, neutral)

**Style Reference:** Sid Meier's Colonization / Port Royale 2 (wooden textures, ornate borders, historical aesthetic).

**Success:** Players say "beautiful UI" not "functional but ugly".

---

### **Month 23: Memory Optimization Pass (HARD LIMITS)**

**Goal:** Controlled memory footprint for 1M entities.

**Deliverables:**
- [ ] **Memory Tracker:**
  ```cpp
  #if defined(SFML1_PROFILE)
  class MemoryBudget {
      void checkLimit(size_t current, size_t limit, const char* name) {
          if (current > limit) {
              LOG_PANIC("Memory budget exceeded: {} > {} for {}", 
                        current, limit, name);
          }
      }
  };
  #endif
  ```
- [ ] **Component Size Audit:**
  - Hot components: <64 bytes (cache-line friendly)
  - Cold components: <256 bytes (rarely accessed)
  - Document: `sizeof(Component)` in header comments
- [ ] **Sparse Component Analysis:**
  - Identify components present in <10% entities
  - Move to separate storage (EnTT `sparse_set`)
- [ ] **Memory Budgets:**
  - ECS components: <2GB (1M entities)
  - Texture atlases: <1GB (100K unique textures)
  - Spatial index: <512MB
  - **Total target:** <4GB for full game state

**Success:**
- [ ] 1M entities @ <4GB RAM
- [ ] Profile mode asserts on budget violations

---

### **Month 24: Performance Final Pass**

**Deliverables:**
- [ ] **Profiling (Tracy):**
  - Identify bottlenecks (top 5 slowest systems)
  - Hot path optimization ONLY (no premature optimization)
- [ ] **LOD System (if needed):**
  - Distant provinces = reduced detail (statistical simulation)
  - Close provinces = full simulation
  - Threshold: distance from camera or player-controlled provinces
- [ ] **Regression Tests (CI):**
  - Track FPS/turn time over commits
  - Red build on >10% regression

**Success Metrics:**
- [ ] **144 FPS minimum** @ 20 civs, 5K units, 10K provinces (MVP scale)
- [ ] **60 FPS minimum** @ 100 civs, 50K units, 50K provinces (mid-scale)
- [ ] **30 FPS minimum** @ 1M entities (full scale) — **WAIT, STRIKE THIS! NO 30 FPS!**

**CORRECTION (per user requirement):**
- [ ] **144 FPS minimum** @ 20 civs, 5K units, 10K provinces (MVP scale)
- [ ] **144 FPS target** @ 100 civs, 50K units, 50K provinces (mid-scale)
- [ ] **60 FPS minimum** @ 1M entities (full scale, absolute floor)

**Note:** Target 1M entities @ 144 FPS if possible (AAA bar). Accept 60 FPS as absolute minimum.

---

## **PHASE 6 — Advanced Tooling & Early Access Prep** (Months 25-33)

**Goal:** Content production, QA, Steam Early Access release.

---

### **Month 25-30: Content Production**

**Deliverables:**
- [ ] **Historical Scenarios:**
  - 10+ start dates (1444, 1776, 1914, 1939, 2020, etc.)
  - Custom events, starting conditions per scenario
- [ ] **Civilization Content:**
  - 20-30 playable civs (unique traits, starting bonuses)
  - Civ-specific units, buildings, technologies
  - Leader portraits, civ flags, flavor text
- [ ] **Province History:**
  - Text descriptions (hundreds of thousands of lines)
  - Historical ownership changes, events, culture
- [ ] **Localization:**
  - English (primary)
  - Russian (secondary)
  - Framework for community translations

---

### **Month 28-30: Tutorial & UX Polish**

**Deliverables:**
- [ ] **Interactive Tutorial:**
  - Step-by-step 4X basics (production, expansion, diplomacy, war)
  - Tooltips, context help
- [ ] **Accessibility:**
  - Colorblind modes (map overlays, civ colors)
  - Font scaling (UI text size options)
  - Audio cues (important events)
- [ ] **Performance Settings:**
  - Graphics quality (low/medium/high/ultra)
  - Entity LOD (automatic or manual)
  - Texture quality (atlas resolution)

---

### **Month 31-33: Civ III-Style Editor**

**Goal:** Player-favorite editor for modding.

**Deliverables:**
- [ ] **Map Editor:**
  - Province placement (Voronoi/grid)
  - Terrain painting (mountains, forests, water)
  - Resource placement
- [ ] **Scenario Editor:**
  - Starting conditions (civ placement, tech levels, armies)
  - Event triggers, custom victory conditions
- [ ] **Data Editor:**
  - Unit/Building/Tech stats (JSON editing in GUI)
  - Tables, dropdown menus, validation
- [ ] **Style:** Civ III editor inspiration (data tables, intuitive layout)

**Why:** Accessible modding tools = community longevity.

---

### **Month 31-33: QA & Optimization**

**Deliverables:**
- [ ] **Replay Viewer:**
  - Load saved replay (input log)
  - Playback at variable speed (1x, 2x, 10x)
  - Visual debugging (entity inspector, component viewer)
- [ ] **Debug Visualization:**
  - Spatial index cells (overlay on map)
  - AI decision nodes (visual graph)
  - Performance heatmaps (which provinces are slow)
- [ ] **Optional QA Tools:**
  - Save corruption fuzzer (randomized load/save, partial data loss)
  - Version mismatch tests (load v1.0 save in v1.5)

---

## 🚀 **EARLY ACCESS RELEASE: Titan 4X (Month 33)**

**Platform:** Steam  
**Price:** $25-30  
**Content:**
- 20-30 civilizations
- 10,000 provinces (Earth map)
- 100+ technologies
- 10+ historical scenarios
- Turn-based multiplayer foundation (2-4 players, lockstep, basic)

**Revenue Goal:** $50K-200K → fund full development (Year 2+)

---

## **PHASE 7 — Post-Release Evolution** (Year 2+)

**Goal:** Continuous development based on Early Access feedback.

---

### **Year 2, Q1-Q2: Neural AI Integration (OPTIONAL)**

**Goal:** Scientific-grade AI (experimental, NOT required for gameplay).

**Deliverables:**
- [ ] **ONNX Runtime Integration:**
  ```cpp
  class NeuralAISystem {
      Ort::Session mModel;
      
      void inferDecisions(World& world, std::span<Entity> civs) {
          // Batch inference: 1000 civs at once
          Ort::Value inputTensor = gatherInputs(world, civs);
          auto output = mModel.Run(runOptions, inputNames, &inputTensor, 1,
                                   outputNames, 1);
          applyDecisions(world, civs, output);
      }
  };
  ```
- [ ] **Heuristic Baseline:** ALWAYS available (fallback)
- [ ] **Player Setting:** Neural AI opt-in (default = heuristic)
- [ ] **Training Pipeline:** Offline training on historical data
- [ ] **NEVER Gate Gameplay:** Neural AI = enhancement, not requirement

**Why:** Scientific depth + competitive AI. But heuristic AI = 80% quality already.

---

### **Year 2, Q2-Q3: Scale to 1M Entities**

**Trigger:** Content expansion (50+ civs, 50K provinces, 100K units).

**Deliverables:**
- [ ] **Job System Full Utilization:** 30+ threads (Ryzen 9 7945HX)
- [ ] **LOD System:** Distant provinces = statistical simulation (O(1) per province)
- [ ] **Streaming:** Chunk load/unload (only active regions in memory)
- [ ] **Memory Budgets:** Strict enforcement (<8GB total for 1M entities)

**Success:**
- [ ] 1M entities @ 60 FPS minimum (144 FPS target)
- [ ] <8GB RAM usage

---

### **Year 2, Q3-Q4: Full Modding Tools**

**Deliverables:**
- [ ] **Paradox-Style Scripting Language (or Lua):**
  - Event scripting (triggers, effects, conditions)
  - Custom game rules
  - Mod conflicts resolution
- [ ] **Event Editor:**
  - GUI for creating events (no manual JSON)
  - Trigger builder (drag-drop conditions)
  - Effect builder (modify game state)
- [ ] **Steam Workshop Integration:**
  - One-click mod install
  - Auto-update
  - Mod ratings, comments

**Why:** Community content = game longevity.

---

### **Year 3+: Additional Planets**

**Deliverables:**
- [ ] **Mars Map:**
  ```cpp
  mPlanetaryMap.addPlanet(PlanetID::Mars, marsConfig);
  ```
  - Wasteland + terraforming mechanics
  - Space travel between Earth/Mars
- [ ] **Venus, Moon, etc.:**
  - Incremental planet additions
  - Unique mechanics per planet (Venusian atmosphere, Lunar low gravity)

**Why:** Multi-planetary empire scale (infrastructure ready since Month 7).

---

### **Year 3+: Dedicated Server (8+ Players)**

**Deliverables:**
- [ ] **Headless Server Build:** (already exists from Month 12)
- [ ] **Client-Server Architecture:** (replaces lockstep peer-to-peer)
- [ ] **Authoritative Server:** Anti-cheat, trusted simulation
- [ ] **Lobby System:** Matchmaking, ranked games
- [ ] **Spectator Mode:** Watch games in progress

**Why:** Scalable multiplayer (8-16 players, dedicated hosting).

---

### **Year 4+: Extreme Performance (1M Entities @ 144 FPS)**

**Trigger:** Content critical mass (200+ civs, 100K provinces, 500K units).

**Deliverables:**
- [ ] **Custom Allocators:** Pool/arena allocators (reduce fragmentation)
- [ ] **SIMD Optimizations:** AI batch processing (AVX2/AVX-512)
- [ ] **GPU Compute:** Pathfinding, influence maps (OpenCL/CUDA)
- [ ] **Extreme LOD:** Background civs = O(1) simulation (statistical models)

**Success:**
- [ ] 1M entities @ 144 FPS
- [ ] 100K unique textures rendered

**Why:** AAA-grade performance at unprecedented scale.

---

## 📊 **SUCCESS METRICS**

---

### **SkyGuard (Month 6)**
- [ ] **240 FPS** on mid-range hardware (GTX 1060 / RX 580)
- [ ] **60 FPS** on integrated graphics (Intel UHD 630)
- [ ] 1v1 multiplayer <100ms latency feel
- [ ] Codebase clean for Titan (no hacks in `core/`)
- [ ] $10K-50K Steam revenue

---

### **Titan 4X Prototype (Month 24)**
- [ ] **144 FPS minimum** @ 20 civs, 5K units, 10K provinces
- [ ] 100K sprites rendering <2ms (GPU)
- [ ] Simulation tick (10K units) <10ms (CPU)
- [ ] Memory usage <4GB for full MVP game state
- [ ] Deterministic replay (100% exact match across platforms)

---

### **Titan 4X Production (Year 2+)**
- [ ] **144 FPS target** @ 100 civs, 50K units, 50K provinces
- [ ] **60 FPS minimum** @ 1M entities (absolute floor)
- [ ] **144 FPS stretch goal** @ 1M entities (AAA bar, Year 4+)
  - **NOTE:** This is aspirational, not mandatory. Don't let it become psychological pressure in Year 1-2. Focus on 60 FPS @ 1M first, then optimize if time/resources allow.
- [ ] Neural AI competitive with human players (optional)
- [ ] Modding ecosystem active (Steam Workshop >100 mods)
- [ ] Multi-planetary scale working (Earth + Mars + Venus)

---

## 🧠 **CORE PRINCIPLES (NON-NEGOTIABLE)**

1. **Performance-First:** Every design decision optimizes for 1M entities @ 144 FPS
2. **Determinism-First:** Simulation = reproducible (replay, saves, multiplayer)
3. **Validate on Write, Trust on Read:** Config loaders validate, runtime trusts
4. **RAII Everywhere:** No manual memory management, no leaks
5. **Zero-Overhead Abstractions:** Release builds = no debug bloat
6. **Clean Release Builds:** Assertions, logging, profiling = compile-time gates
7. **Data-Driven Design:** JSON configs, binary saves, moddability foundation
8. **AAA-Grade Quality:** No compromises on architecture, performance, UX

---

## ❌ **NON-GOALS (EXPLICIT)**

- ❌ No ImGui requirement (custom ECS UI for artistic control)
- ❌ No ECS-only UI dogma (hybrid: ECS scene graph + non-ECS virtualization)
- ❌ No raw OpenGL rewrite unless profiling demands (SFML adequate for 1M entities)
- ❌ No premature chunk serialization (EnTT snapshot sufficient until Year 2)
- ❌ No neural AI dependency for gameplay (heuristic baseline ALWAYS works)
- ❌ **NO 30 FPS ANYWHERE** (60 FPS absolute floor, 144 FPS target, 240 FPS SkyGuard)

---

## 📝 **ROADMAP USAGE**

- **Delete completed tasks** as you finish them
- **Add notes** directly in sections (progress tracking)
- **Update timelines** if priorities shift
- **Version control** this document (git history = project memory)

---

**END OF ROADMAP v2.2**

Approved for production. Let's build something legendary.
