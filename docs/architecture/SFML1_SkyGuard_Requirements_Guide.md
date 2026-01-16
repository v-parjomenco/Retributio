# SFML1 / SkyGuard
## Architectural Requirements Guide
**Version 1.1 — January 2026**

**Tech Stack:** C++20, SFML 3.0.2, nlohmann-json 3.12, EnTT 3.16.0  
**Primary Target:** Flagship 4X grand strategy game  
**Prototype:** SkyGuard (aerial sim for validation)  
**Quality Bar:** AAA production-grade, commit-ready code only

---

## 1. Project Identity & Goals

SFML1 is a high-performance 2D engine for large-scale turn-based grand strategy games, targeting flagship 4X quality. The engine is flexible enough for side projects (platformers, small RPGs) for rapid development and funding, but is optimized and architected specifically for 4X grand strategy with 100K+ entities, complex AI, and scientific simulation.

SkyGuard is the first prototype: a small aerial simulator serving as a validation lab for ECS architecture, resource management, performance, and UI systems. SkyGuard will never reach 50K entities—it uses stress tests with virtual entities to validate scalability for the flagship 4X.

**Core Principles:**
- **Performance-first:** If in doubt, prioritize performance
- **AAA quality bar:** Production-grade, commit-ready code only
- **Data-driven:** JSON is source of truth for content and configuration
- **Zero overhead:** Avoid defensive branching in hot paths
- **Deterministic by design:** For saves, replay, and future multiplayer

---

## 2. Core Architecture Principles

### 2.1 Core/Game Separation

`core/` is engine-only, game-agnostic, and reusable across projects. `game/*` (e.g., `game::skyguard`) is game-specific and never pollutes core. Clear boundaries enable engine reuse and prevent coupling.

### 2.2 Modularity

Each module/file has one clear responsibility. Build from small autonomous bricks with minimal dependencies. No god files, no mega-modules, no spaghetti.

### 2.3 Zero-Overhead Mindset

Avoid virtual/type-erasure in hot paths. Prefer data-oriented layouts and inlining. When in doubt, keep hot-path cost minimal and predictable. Handle failures at boundaries via logging, fallbacks, or fail-fast.

### 2.4 Validate on Write, Trust on Read

Validate only at mutation boundaries (loading, authoring, applying config). Release builds must not pay recurring validation costs in hot paths. Debug-only asserts are allowed and encouraged.

### 2.5 Data Contract

The data layer is the source of truth for moddable, large-scale content.

Resource identity is defined by the Resource Registry v1 contract:
- **Canonical ID:** Canonical Key String (namespaced, human-readable, mod-friendly).
- **Persistence ID:** StableKey64 derived deterministically from the canonical key string (frozen algorithm/seed for v1).
- **Runtime handle:** RuntimeKey32 used in hot paths (ECS components, rendering). Hot paths must not use strings/hashes/maps.

Compile-time enums are not a core contract for resources. Enums may exist only as local convenience in game/prototype code, but must not be required for mods, persistence, or engine-level resource identity.

---

## 3. Repository Structure

Single repository containing engine core, game layers, and assets/configs split by core/game. Dev-only utilities (stress scenes) live in game layer, never in core, with appropriate Debug/Profile guards.

**Structure:**
- `SFML1/include/core/` — Engine headers
- `SFML1/src/core/` — Engine implementation
- `SFML1/include/game/` — Game-specific headers
- `SFML1/src/game/` — Game-specific implementation
- `SFML1/assets/core/` — Engine assets (fonts, core configs)
- `SFML1/assets/game/` — Game assets (textures, sounds, game configs)
- `SFML1/include/third_party/` — Third-party wrappers

---

## 4. Code Quality & Style

### 4.1 Quality Bar

AAA, commit-ready, production-grade code only. No toy/demo implementations. Follow RAII, YAGNI, DRY principles strictly.

### 4.2 Formatting

- **Indentation:** 4 spaces, no tabs
- **Braces:** Same line (`if (...) {`), always use braces even for single-line
- **Pointer/reference:** `T* ptr`, `T& ref`
- **Line length:** ~100 chars target (avoid routinely exceeding)

### 4.3 Include Order (.cpp files)

1. Corresponding header
2. Standard library
3. SFML headers
4. Other project headers

Separate blocks with blank line; sort alphabetically within each block.

### 4.4 Naming

Clear, descriptive identifiers. Short names only for standard cases (e.g., `dt` for delta time).

### 4.5 Comments & Documentation

Each new/modified header has a single top-of-file banner in English (file path, purpose, used by, related headers, notes). `.cpp` files usually must not have top-of-file banners. No additional banner headers inside files.

In-code comments are in Russian, explain intent/invariants/non-obvious decisions. Comments may be rewritten but must not be silently removed.

### 4.6 Component Headers: Document Size

Component headers should document size in bytes in the Purpose field for cache-line awareness. Example: `'Purpose: ID-based sprite component (hot path, 40 bytes)'`

---

## 5. ECS Core Rules (EnTT Backend)

### 5.1 World / Registry Contract

Backend is EnTT 3.16.0 (`entt::entity`, `entt::registry`). `core::ecs::Entity = entt::entity`; `NullEntity = entt::null`.

World owns exactly one `entt::registry` and a `SystemManager`.

No custom Registry, no custom `ComponentStorage<T>`; all components live in EnTT storage. World API: create/destroy/isAlive; add/get/remove component; view/group; addSystem/update/render. `registry()` is escape hatch only (friend access).

Wrappers must inline to raw EnTT calls (no virtual/type-erasure in hot paths). World must not leak EnTT types outside `core/ecs`.

### 5.2 Component Contract (Performance-First)

Components are POD/SoA-friendly: trivially copyable, trivially destructible, no custom ctor/dtor, no virtuals. Enforced via C++20 concepts.

Components must not own heavy objects/resources. **Forbidden:** `sf::Sprite`, `sf::Texture`, `sf::RenderWindow`, file handles, owning smart pointers. Store only IDs, flags, small numeric/enum types, simple POD structs. Small trivially-copyable value types OK if no per-entity overhead (e.g., `sf::IntRect`).

Deterministic defaults: value-init (`T t{}`) and/or in-class initializers. For default objects always `Type t{}` (never `Type t;`).

### 5.3 Sparse/Heavy State Policy

Rare/heavy state lives in subsystem-owned pools/arenas with explicit lifecycle. ECS components store only small POD handles/indices/IDs into that state.

### 5.4 Systems: Access Declarations & State

Systems declare component access explicitly (read-only vs read-write) to enable future scheduling/parallelization/conflict detection. Systems are essentially stateless (except configuration) and operate on ECS data; external services are injected explicitly, not hidden globals.

### 5.5 Scalability Rules (100K+ Entities)

- Use dirty flags to skip unchanged data (O(dirty) not O(all))
- Keep rare state sparse (don't spread rare data across every entity/component)
- Update spatial indices incrementally (only moved/changed entities)
- **NEVER iterate all entities unconditionally:** iterate only relevant subsets (active/visible/AI-owned), make subset selection explicit

**Anti-pattern example:** `for (all provinces) check distance to capital`. **Correct:** `spatialIndex.query(radius)` then iterate results.

### 5.6 Spatial Contract (Flagship-Critical)

`core::spatial` is first-class scaling primitive (grid/quadtree, chunked maps, spatial queries). For systems depending on proximity/visibility/perception/broad-phase: brute-force scans are forbidden. Systems must query `core::spatial` and/or derived registries (RenderQueue, ActiveAISet), updated incrementally and driven by dirty flags.

### 5.7 Sprite / Rendering Design

SpriteComponent is data-only: TextureKey (RuntimeKey32), `sf::IntRect textureRect`, `float zOrder`, `sf::Vector2f scale/origin`. 0-sized textureRect means full texture. No `sf::Sprite` in components.

RenderSystem iterates (TransformComponent, SpriteComponent); resolves textures via ResourceManager; uses transient sprite(s); draw order by zOrder.

### 5.8 Deterministic Ordering

Use external `std::vector<entt::entity>` + `std::sort`. Pattern: build view once; collect entities into scratch vector; comparator uses `view.get<T>(entity)`; reuse scratch vectors each frame.

`registry.sort<T>()` only for small mostly static sets and only when data is dirty; never every frame on large sets; never sort same component by different criteria in different systems.

When ordering matters for determinism, use explicit stable ordering key (e.g., stable ID) and prefer shared helpers over ad-hoc per-system sorting.

### 5.9 World Usage Best Practices

Prefer in-place `addComponent<T>(e, args...)` and explicitly init non-empty components. Prefer `tryGetComponent<T>()` for check+use (single lookup); use `getComponent<T>()` for fail-fast (assert). Use `view()`/`view() const` for iteration; `group()` only when needed for hot paths. Do not rely on implicit iteration order when order matters (collect+sort). Use `registry()` only as escape hatch for advanced EnTT features.

### 5.10 EnTT Signals for Cleanup

Use EnTT `on_destroy` signals for automatic resource cleanup to avoid manual bookkeeping. Example: `registry.on_destroy<SpatialHandleComponent>().connect<&SpatialIndexSystem::onDestroyed>()`. This is RAII for ECS—cleanup happens automatically without manual calls in destroy paths.

---

## 6. Resource Management

### 6.1 Resource Layer Rules

Resource identity follows the Resource Registry v1 contract:
- **Canonical ID:** Canonical Key String (namespaced).
- **Persistence ID:** StableKey64 derived from the canonical key string (frozen for v1).
- **Runtime handle:** RuntimeKey32 used in hot paths.

**Canonical Key String Format (frozen contract):**
- Format: `namespace.category.name` (minimum 3 segments, separator `.`).
- Each segment: `[a-z][a-z0-9_-]*`; the entire key is lowercase; allowed chars: `[a-z0-9._-]`.
- Regex: `^[a-z][a-z0-9_-]*(\.[a-z][a-z0-9_-]*){2,}$`.
- Validation is mandatory at registry build (validate-on-write). Invalid canonical keys are authoring errors.

**Pipeline:**
- `resources.json` defines registries and load parameters.
- The runtime registry is built at load-time with validate-on-write; runtime code trusts resolved handles.

**Layering:**
- ResourceManager is the façade (owned by the composition root, no globals).
- ResourceHolder remains the cache.
- Registry internals (key storage, indices, path mapping) are private to the resource subsystem: no code outside ResourceManager and bootstrapping/composition root may depend on them.

### 6.2 Resource Registry Policy

**Type A** (critical, textures/fonts): PANIC on any registry write/error. `LOG_PANIC` is `[[noreturn]]`; do not add terminate/exit/throw after calling it.

**Type B** (important with safe defaults): I/O failure → WARN + defaults; JSON failure → critical or non-critical depending on whether it's tuning-only.

**Type C** (dev/cosmetic): I/O failure → WARN + defaults; JSON failure → non-critical fallback; never crash.

Sounds are always soft-fail (WARN + skip); never PANIC due to missing/invalid sound assets.

### 6.3 Render Hot Path Rule

ResourceManager::getTexture() must not be called per-sprite or on every texture switch. Resolve TextureKey (RuntimeKey32) → `sf::Texture` via a per-frame cache (cache unique textures once per frame).

**Hot-path constraint:**
- Render hot loops must use O(1) handle-based access only (no strings, no hashing, no maps).
- Any per-frame cache used for textures must be index-based (by RuntimeKey32 index) to keep zero-overhead per sprite.
- Per-frame uniqueness caching MUST NOT use associative containers in render hot loops (e.g., `unordered_map`). Use an index-addressed epoch/stamp mechanism keyed by `RuntimeKey32.index()`.

### 6.4 Deterministic Resource Registry Build & Loading

The resource registry build must be deterministic for reproducible behavior (logs, bugs, allocations, saves/replays).

**Deterministic order:**
- The winning set of resources is ordered by StableKey64 (total order).
- RuntimeKey32 indices are assigned in that deterministic order.

**Override determinism:**
- Duplicate canonical keys across sources are expected and must resolve deterministically by explicit source priority/order.
- Duplicate canonical keys within a single source are an authoring error and must fail validation at load time.

---

## 7. Configuration & JSON Loading

### 7.1 JSON Parsing Contract

JSON parsing+validation helpers: `core::utils::json::parseAndValidateCritical` / `parseAndValidateNonCritical` (in `json_document.h`). Critical mode: `LOG_PANIC` + terminate on error. Non-critical mode: returns `std::nullopt`; detailed parsing/validation logging is DEBUG only.

Non-critical loaders must emit exactly one WARN when `dataOpt` is `nullopt` to describe semantic outcome ("config not applied, using defaults"); avoid duplicate WARN across layers.

Any `noexcept` parsing helper must not call throwing APIs (e.g., `nlohmann::json::get<T>()` when it may throw); use type-checked access + finite/range checks (no-throw path) to avoid `std::terminate`.

### 7.2 Three-Tier Loader Policy

**Type A** (critical, e.g., resource registries): `parseAndValidateCritical` for any I/O or JSON failure; loader does not add extra ERROR/PANIC logs.

**Type B** (important with safe defaults): I/O failure → WARN + return defaults; JSON failure → critical or non-critical depending on whether it's tuning-only.

**Type C** (dev/cosmetic): I/O failure → WARN + defaults; JSON failure → `parseAndValidateNonCritical` fallback; never crash.

### 7.3 JSON Options by Data Provenance

Choose JSON parse/validation options based on where the data comes from:

**Hand-authored configs** (engine/gameplay): reject duplicate keys for determinism/authoring safety; do structural checks once at boundary; prefer field-level `*WithIssue` parsers as single source of truth; keep `SchemaPolicy::Skip` to avoid duplicate checks; enable `SchemaPolicy::Validate` only for all-or-nothing configs or when `*WithIssue` coverage is missing.

**Saves/program-generated/large payloads**: use `kFastOptions` (`DuplicateKeyPolicy::Ignore`; `SchemaPolicy::Skip`) for performance-first behavior; trust generator.

**Tooling/debug paths**: may enable stricter structural checks explicitly; Profile stays release-like unless explicitly requested.

---

## 8. Logging Policy

### 8.1 Severity Levels

Release logs are Info+ only (Debug/Trace off by default, zero perf impact on simulation). Trace is per-frame/tight-loop, very noisy; never Info/Warning. Debug is rare, rate-limited diagnostics; never tight loops. Info is rare milestones safe for Release. Warning is recoverable, actionable, non-spam. Error is malfunction but process may live. Critical is fatal, usually panic/exit.

### 8.2 Stable Categories

Engine, Config, Resources, ECS, UI, Gameplay, Debug/Performance.

### 8.3 Directory ↔ Category ↔ Logic

Each directory/module owns one primary responsibility and has a default logging category aligned with that responsibility. Cross-category logs are allowed only when the meaning genuinely changes; the directory's default category remains the baseline.

**Canonical mapping:** `core/log/*` → Engine; `core/config/*` → Config; `core/resources/*` → Resources; `core/ecs/*` → ECS; `core/ui/*` → UI; `core/time/*` → Engine (service init) or Debug/Performance (profiling); `core/utils/json/*` → Config; `game/skyguard/*` (config) → Config for JSON errors, Gameplay for game outcomes.

---

## 9. Determinism & Saves

### 9.1 Core Determinism Requirements

Simulation determinism is a first-class requirement: fixed system update order, stable iteration rules, seeded RNG, avoid nondeterministic operations. This enables reproducible saves, replay, and future multiplayer.

Resource registry build and resource loading must be deterministic. Deterministic order is defined by StableKey64 (total order) and the explicit source/mod load order used to build the registry.

RNG must be seeded explicitly, state must be serializable. Avoid platform-dependent math (`std::sin`, `std::atan2`) in simulation-critical code—consider deterministic math libraries or fixed-point for combat/economy calculations.

### 9.2 Floating-Point Determinism (Principle)

For simulation code: use consistent FP mode (`/fp:precise` MSVC, `-ffloat-store` GCC). Avoid fast-math optimizations (`-ffast-math`, `/fp:fast`). Consider deterministic math library or fixed-point for critical calculations. Document platform-specific behavior if unavoidable. Unit tests must validate cross-platform consistency. Details of libraries/implementation in RoadMap.

### 9.3 Entity ID Policy

`entt::entity` is RUNTIME ONLY (generation counter, may be reused). Saves/network require stable IDs (separate `uint64` + mapping table). Never serialize `entt::entity` directly to disk or network. See `entity.h:66-68` for detailed explanation. Implementation: when 4X save system is built.

### 9.4 Save Format

Save format is versioned with migrations. Large-scale saves: delta saves (only changed components), compressed/sparse serialization, chunked loading/streaming (entity→chunk mapping + manifest; per-chunk compression). Save metadata includes entity/component counts, playtime, and a game-state hash.

**Resource registry reproducibility:** Save header MUST store the exact source/mod list and deterministic load order (or an equivalent hash) used to build the resource registry. When loading, the engine must rebuild the registry using the save order, or warn and refuse to load if the active source/mod set/order is incompatible.

**Component versioning:** Per-component version (static constexpr in struct). Migration functions for each version bump. Loader must support N-1 and N-2 versions minimum. Implementation: from 4X start.

### 9.5 Move Semantics with Primitive Counters

Custom types with primitive counters must manually zero source in move operations. Default move copies primitives but leaves source with stale values, causing invariant violations. See `world.h:115-119` for critical example.

---

## 10. AI Architecture (Data-Driven + Budgeted)

AI is fully data-driven (doctrines/focus/goals in JSON). Decision hierarchy: strategic (utility scoring chooses goals/foci) → planning (GOAP/HTN-like expansion into actions) → tactical (behavior trees with lightweight utility scoring).

### 10.1 AI Execution Model

Enforced by AI scheduler with explicit budgets (per empire/front/group). Time-sliced execution with explicit resume points (continue next tick/turn when budget is hit). Mandatory metrics (processed/skipped, time spent) exposed to debug overlay/profiler. Specific millisecond budgets (100ms strategic, 50ms operational, 5ms tactical) → RoadMap tuning.

### 10.2 Performance Enforcement

Performance is enforced via caching (utility scores, influence maps, pathfinding) and hierarchical execution (strategic sets goals, tactical executes without re-evaluating strategy every turn).

---

## 11. Turn-Based Simulation Cadence

Turn-based simulation uses layered cadences: tactical systems run every turn; operational/strategic systems run less frequently (configurable) to keep worst-case turn times bounded and predictable.

**Tactical** = units/battles (every turn). **Operational** = fronts/provinces/logistics (every 3 turns). **Strategic** = empire-level goals/diplomacy/ideology (every 10 turns). SystemManager groups systems by layer with configurable update frequencies.

---

## 12. Performance Targets & Budgets

Fundamental rule: performance is enforced via explicit budgets and LOD/time-slicing, not by hoping "fast enough". Hard constraints: no recurring "hypothetical-case" checks in hot paths (Release); diagnostics exist but must be cheap-by-default (Debug/Trace gated, rate-limited, or moved to overlay/profiler).

**Performance-First Main Idea:**  
If there is any doubt, treat performance as the primary priority and act accordingly.

---

## 13. Job-Friendly Design & Concurrency

Implementation is single-threaded for now, but job-friendly by design: no global mutable state; data owned by World/Game; systems grouped into phases/layers for future scheduling. Phase-based execution: fixed phase order (deterministic); system dependencies derived from declared component access (read vs write); may run single-threaded today, but API and dataflow must remain job-friendly.

---

## 14. Future-Proofing (4X-Ready Design)

### 14.1 Allocator-Aware API

API must be allocator-aware for future custom allocators (when 4X reaches 100K+ entities). Use allocator-aware containers (prefer `std::vector` over raw `new[]`). Use template aliases for easy swapping:

```cpp
template<typename T> using game_vector = std::vector<T, std::allocator<T>>;
```

Current implementation uses `std::allocator` everywhere (zero custom allocator overhead). Custom allocators implementation: when profiling shows allocation bottleneck in 4X (RoadMap).

### 14.2 Resource ID Stability (Mod Compatibility)

For resources, the stability contract is defined by Canonical Key Strings:
- Canonical Key Strings must not be removed or renamed for mod compatibility.
- StableKey64 derived from the canonical key string provides cross-version stability for saves, replays, and mod manifests.

Changing the StableKey64 derivation (algorithm/seed) is a breaking change and requires an explicit migration strategy.

### 14.3 Multiplayer-Friendly Architecture

Design for future multiplayer but don't implement now. Architecture must support: deterministic simulation, stable entity IDs, versioned saves, checksum validation. Actual multiplayer netcode: RoadMap.

---

## 15. RoadMap Items (Future Implementation)

The following are planned features, not current requirements. See separate RoadMap document for details:

- Event system (pub-sub, history timeline, scriptable triggers/effects)
- Hot-reload (file watching + JSON config reload without restart)
- Custom allocators implementation (pools/arenas)
- Chunked maps / LOD system
- Scripting language layer (Lua or custom)
- Fixed-point math libraries for deterministic calculations
- Multiplayer netcode
- AI budget tuning (specific millisecond values)
- Component versioning system (when 4X saves implemented)
- Stable entity ID mapping (when 4X saves implemented)

---

## Appendix: Key Implementation Patterns

### A.1 Deterministic Resource Preloading

Resources are preloaded in a deterministic order defined by StableKey64 (total order), ensuring stable logs, reproducible bugs, and predictable allocation/loading behavior across runs given the same registry inputs and source/mod order.

### A.2 Move Semantics Manual Zeroing

Types with primitive counters must manually zero source in move operations to prevent invariant violations. See `world.h:115-119` for critical example (`mAliveEntityCount` synchronization).

### A.3 Component Size Documentation

Component headers document size in bytes in Purpose field for cache-line awareness. Example: `'Purpose: ID-based sprite component (hot path, 40 bytes)'`. Target <64 bytes for hot components (1 cache line).

### A.4 EnTT on_destroy Signals

Use EnTT `on_destroy` signals for automatic resource cleanup to avoid manual bookkeeping. This is RAII for ECS. See `spatial_index_system.cpp:96-98` for example.

---

**— End of Document —**