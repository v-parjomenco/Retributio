# Retributio — Spatial Chunking & SpatialIndex v2 Contract (Atrapacielos + Auctoritas) — v2.0
Approved: 26.01.2026

Status: **Approved, commit-ready architecture contract**  
Audience: engine/core + game layers (Atrapacielos today, Auctoritas later)  
Language: C++20, performance-first; **Auctoritas is determinism-first**.

---

## 0. Purpose

This document defines non-negotiable engine contracts for:

- **SpatialIndex v2**: broad-phase spatial queries without hot-path hashing/allocations.
- **Chunk world coordinate contract**: single source of truth for streaming, LOD, AI, and rendering.
- **TileMap chunking contract** for Auctoritas-style tile worlds (future), without polluting Atrapacielos.

It avoids Auctoritas gameplay/balance configs and does not assume a Auctoritas executable exists today.  
It defines tunable parameters + validation harness so final values are chosen by measurement.

---

## 1. Glossary

- **World units**: canonical unit for simulation/spatial queries.
  - Atrapacielos: simulation may remain `float`, but partitioning must be robust for large coordinates.
  - Auctoritas: world units are **pixels** (`tileSizePx = 256`) in simulation.

- **World chunk**: coarse partition of world space identified by `ChunkCoord {x,y}`.
  - Chunk size is not a world bound; it is a partition size.
  - World may be unbounded (Atrapacielos) while using fixed-size chunks.

- **Spatial cell**: fine partition inside a spatial chunk used by SpatialIndex candidate gathering.

- **Tile sub-page**: tilemap-only micro-chunk used to rebuild mesh/vertex-buffer ranges locally.

- **Proxy**: zoom-out representation of a chunk (baked texture / simplified mesh) used for strategic rendering.

---

## 2. Non-negotiable rules (hard constraints)

### 2.1 Measure before optimizing
- Instrument `SpatialIndex::queryFast()` and `SpatialIndex::queryDeterministic()` (and other hot systems) with profiler scopes.
- Provide a **headless 4X-scale harness** (Debug/Profile only) to validate risks without building Auctoritas gameplay.

### 2.2 SpatialIndex contracts
- `queryFast()` and `queryDeterministic()` must not allocate.
- Read-path must not trigger container growth, rehash, or pool growth.
- Write-path (register/update/remove, prewarm/reserve) may perform **controlled allocations** (capacity growth, marks resize, overflow pool growth) in **budgeted phases**.
- Update paths for dirty entities must avoid surprise allocations inside hot loops.
- No per-cell hashing in `query*()`; hashing is allowed only at **chunk** level for sparse/unbounded worlds.
- Must support both:
  - bounded flat chunk addressing (Auctoritas),
  - bounded-capacity sparse active window (Atrapacielos).

### 2.3 TileMap contracts (Auctoritas)
- Tiles are not ECS entities.
- Tile rendering uses chunked geometry (vertex buffers/arrays), rebuilt at sub-page granularity.
- TileMap, TileData, and SpatialIndex are separate systems that share `ChunkCoord`, not implementation.

### 2.4 Residency / streaming
- “All chunks in memory” is forbidden for high-detail payloads.
- Residency is layer-based:
  - Always-resident: compact global aggregates (ownership, coarse nav graph, etc.).
  - Streamed: high-detail visuals, GPU buffers, per-tile decorations, city-era variations.

### 2.5 Practicality gate (Atrapacielos)
If a feature is Auctoritas-critical but Atrapacielos cannot meaningfully validate it, we do:
- interface + invariants + profiler hooks now,
- implementation later (Auctoritas prototype phase).

No invented APIs with no validation path.

---

## 3. Determinism & numeric representation (Auctoritas)

Auctoritas is turn-based and requires deterministic replays. Simulation must not depend on:
- nondeterministic iteration order from hash containers,
- platform-dependent floating-point behavior.

### 3.1 Auctoritas world-space representation
Contract:
- Auctoritas simulation uses **integer pixel coordinates** (`int32_t`) for positions and AABBs where feasible.
- Rendering uses float coordinates **relative to camera** (presentation only).

AABB contract (Auctoritas simulation):
- `Aabb2i { int32_t minX, minY, maxX, maxY }` uses **half-open** bounds `[min, max)`.
- All intersection and coverage functions must use the same half-open rule.
- Auctoritas integer AABB semantics must not be mixed with Atrapacielos float AABB semantics.

Atrapacielos may keep float AABBs; Auctoritas does not.

### 3.2 Deterministic query mode

SpatialIndex provides two query modes:
- **Fast/Unordered**: rendering and non-simulation use; order is implementation-defined.
- **Deterministic**: simulation-critical selection; output order must be deterministic for fixed state and fixed seeds.

Deterministic mode requirements:
- deterministic traversal order of chunks and cells,
- deterministic handling of overflow lists,
- stable output ordering by a deterministic key (**Auctoritas uses StableID, never `entt::entity`**).

Deterministic mode must be hard to misuse:
- API exposes separate methods: `queryFast()` and `queryDeterministic()`.
- In Auctoritas Debug: calling `queryFast()` from simulation scope must assert/fail.
- In Auctoritas Profile/Release: calling `queryFast()` from simulation scope must be a deterministic fail-fast (log critical + terminate).

### 3.3 StableID subsystem (mandatory for Auctoritas)

- Auctoritas requires `StableId = uint64_t`, monotonically increasing and never reused.
- StableID is provided by a **core service** (side-table), not ECS components:
  - O(1) lookup by `entt::entity` raw index.
- **Reuse / generation contract**:
  - Side-table entry stores the `entt::entity` generation/version observed at assignment.
  - Lookup validates the generation matches; otherwise the entry is invalid (stale handle).
  - On entity destruction (during `flushDestroyed()`), the side-table entry is cleared/invalidated.
- StableID assignment order must be deterministic:
  - Auctoritas must enforce deterministic entity creation order (no spawning from unordered iteration without sorting by deterministic keys).

Atrapacielos:
- StableID service may be present but is not required; Atrapacielos does not use deterministic query mode.

### 3.4 Deterministic ordering mechanism (Auctoritas, zero-overhead)

Auctoritas uses **stable traversal + deterministic insertion** (no per-query sorting):
- Dirty entities are sorted by StableID before SpatialIndex write-phase updates.
- Cell insertion preserves this sorted order.
- Overflow spans preserve insertion order.
- Chunk traversal is deterministic (flat storage: increasing linear index).
- Cell traversal is deterministic (row-major: y then x).

Optional Debug-only validation:
- A debug-only pass may post-sort results by StableID and compare against traversal order to detect bugs.


---

## 4. Concurrency contract (future-proof)

Core is single-threaded today, but must remain job-friendly.

Contract (phase model):
- **Write phase**: updates/inserts/removes.
- **Read phase**: queries.

Rules:
- Queries are thread-safe only when the index is immutable and no writes occur concurrently (barrier or snapshot).
- Writes are not thread-safe by default.
- Future parallelism is enabled by per-chunk independence and explicit phase barriers.

Disjoint-chunk parallel writes (future):
- allowed only if work partitioning guarantees no two threads write the same chunk concurrently,
- per-chunk locks are a possible future implementation, but not required now.

---

## 5. Unified chunk coordinate contract

### 5.1 Location
`ChunkCoord` lives in core:

`engine/include/core/spatial/chunk_coord.h`  
Namespace: `core::spatial`.

### 5.2 Definition
- `ChunkCoord` is tile-agnostic.
- It represents chunk-grid coordinates in world units.

Required API surface:
- `ChunkCoord worldToChunk(WorldPos p, int32_t chunkSizeWorld) noexcept`
- `WorldPos chunkOrigin(ChunkCoord c, int32_t chunkSizeWorld) noexcept`
- `ChunkRect chunkBounds(ChunkCoord c, int32_t chunkSizeWorld) noexcept`

`WorldPos` is a small POD chosen by the game layer (float for Atrapacielos; int32 pixels for Auctoritas).

### 5.3 Clarification: 128 * 256 vs 256 * 256
If `tileSizePx = 256`:
- Chunk edge in pixels = `chunkSizeTiles * tileSizePx`
- For `chunkSizeTiles = 128` → `chunkSizePx = 128 * 256 = 32768`

`256 * 256` is tile area, not edge length.

---

## 6. SpatialIndex v2 — architecture

### 6.1 Goals
- Stable frametimes: eliminate hot-path `unordered_map<CellCoord, ...>` lookups.
- Deterministic iteration in deterministic mode (Auctoritas).
- One algorithm usable by Atrapacielos and Auctoritas via specialized storage backends.

### 6.2 Chunked dense grid
SpatialIndex partitions as:
- chunk level: locate intersecting chunks (sparse or flat addressing),
- inside chunk: dense 2D grid of cells using direct indexing.

### 6.3 Cell size (“512”) — what it means
`cellSizeWorld` is tunable.

Auctoritas:
- `cellSizePx = 512` equals 2×2 tiles because 2 * 256 = 512 (starting point).

Atrapacielos:
- `cellSizeWorld` remains tunable; projectiles may prefer smaller cells.

### 6.4 Cell layout: cache-line sized and cache-line aligned

SpatialIndex v2 stores a stable 32-bit identifier internally to keep the layout predictable and cache-friendly.

#### 6.4.1 EntityId32 contract
- SpatialIndex internally stores `EntityId32 = uint32_t`.
- Auctoritas builds require a 32-bit entity backend:
  - `static_assert(sizeof(entt::entity) == 4, "Auctoritas requires 32-bit entt::entity backend");`
- If a 64-bit entity backend is ever required, SpatialIndex v2 layout must be redesigned (this contract is intentionally strict).

#### 6.4.2 Cell cache-line contract
Cell is exactly one cache line (64 bytes) and aligned to a cache-line boundary.

Contract:
```cpp
using EntityId32 = std::uint32_t;

struct alignas(64) Cell final {
    static constexpr std::uint8_t kInlineCapacity = 14; // derived from cache-line packing

    std::array<EntityId32, kInlineCapacity> entities{}; // 56 bytes
    std::uint8_t count = 0;

    std::uint8_t pad0 = 0;
    std::uint8_t pad1 = 0;
    std::uint8_t pad2 = 0;

    std::uint32_t overflowHandle = 0; // 0 == none (pool handle)
};

static_assert(sizeof(Cell) == 64, "Cell must fit exactly one cache line.");
static_assert(alignof(Cell) == 64, "Cell must align to cache-line boundary.");
```

Capacity rationale (documentation):
```cpp
constexpr std::size_t kCacheLine = 64;
constexpr std::size_t kOverhead = 1 + 3 + 4; // count + padding + handle
constexpr std::size_t kInlineCapacity = (kCacheLine - kOverhead) / sizeof(EntityId32);
static_assert(kInlineCapacity == 14);
```

Notes:
- Civ-style tile stacks must not force large cell densities. Auctoritas may store stack membership in gameplay/tile data; SpatialIndex density stays low for most cells.
- Overflow is required for clustering and multi-cell AABBs, but must never allocate in `query*()`.


### 6.5 SpatialChunk storage (no growth)
```cpp
struct SpatialChunk final {
    std::vector<Cell> cells; // fixed size after load; never grows
    std::uint32_t activeEntityCount = 0;
};
```

---

## 7. Overflow pool contract (deterministic, bounded, non-allocating in query)

Overflow exists to handle rare high-density cells.

### 7.1 Structure
Contract-level structure (example shape, not a mandated type):
- pooled storage `std::vector<std::vector<Entity>>` (or equivalent) indexed by `overflowHandle`,
- freelist of unused handles,
- optional per-overflow vector `reserve` policy.

### 7.2 Determinism
In deterministic mode:
- overflow traversal order is deterministic,
- insertion order into overflow must be deterministic (Auctoritas: order by StableID, or deterministic update order + stable per-cell insertion policy).

### 7.3 No allocations in query
- `query*()` may read overflow spans, but must never allocate.
- Overflow growth may occur only in controlled write paths (load/prewarm/update), never in read phase.

### 7.4 Exhaustion / failure mode (explicit)

Overflow pool is bounded by a configurable ceiling (Auctoritas) or budget (Atrapacielos).

When an insert/update requires overflow capacity and the pool cannot provide it:

**Auctoritas**
- Debug: assert + log critical (diagnostic is mandatory).
- Release: deterministic failure policy is required:
  - default: log critical + terminate.
  - optional: grow pool **only in a budgeted/prewarm phase** (streaming/maintenance), never inside query and never inside mid-simulation hot loops.

**Atrapacielos**
- Release: allowed soft-fail policy (log warning/error and skip insertion for that entity) if configured.

The failure mode must be explicit, deterministic, and test-covered in harness.


---

## 8. Query correctness contracts

### 8.1 Multi-cell AABB dedup (contract-level choice)

SpatialIndex must provide duplicate-free results.

Contract: stamping-based dedup is mandatory:
- Dedup uses a per-query stamp counter (`uint32_t`, wraps).
- Stamp storage lives in a SpatialIndex **internal marks table** indexed by `EntityId32`.
- Stamp table growth/resizes happen only in write-path (e.g., `registerEntity()` / prewarm), never in `query*()`.

Stamp overflow handling:
- on `uint32_t` wrap, reset via epoch technique or a controlled full clear in a non-hot phase (documented and tested).

This is O(1) per candidate and allocation-free in `query*()`.


### 8.2 Entity lifetime during query usage
Contract:
- Entity destruction is **deferred** until the end of the current phase/tick barrier.
- During a read phase, callers must treat query results as valid for the remainder of that phase.
- Immediate removal from gameplay/render visibility must be expressed via state (e.g., Disabled/Hidden tag) rather than immediate destruction.

Notes:
- `isAlive()` checks may be used as Debug-only assertions, but are not the correctness mechanism.
- Deferred destruction is mandatory for Auctoritas safety and deterministic replays.


## 9. Updates: dirty entities only (deterministic in Auctoritas)

SpatialIndexSystem processes only dirty entities:
- remove previous coverage,
- insert new coverage,
- clear dirty tag.

Auctoritas deterministic update order:
- collect dirty entities and sort by StableID before applying updates (requires StableID service).

No global scans.

---

## 10. Chunk storage backends (performance-critical)

### 10.1 Auctoritas bounded maps: flat addressing + payload pooling
- flat slot table (always resident),
- payload pool for heavy chunk data (allocated only when loaded).

O(1), deterministic traversal, stable memory behavior.

### 10.2 Atrapacielos unbounded scrolling: bounded-capacity active window (specialization)

Atrapacielos is unbounded in -Y, but the active window is small and camera-follow driven.

Requirements:
- reserve capacity up-front,
- bounded payload pool,
- no surprise growth inside hot loops.

Chosen strategy: sliding window ring-buffer with origin shifting:
- window size = `maxActiveChunks` (configured at init),
- O(1) direct indexing (no chunk-level hashing in hot paths):
  - `local = chunkCoord - windowOrigin`
  - `slot = (local.y * windowWidth + local.x)` (windowWidth derived from window dimensions; 1D corridor is allowed)
- window shifts when the camera/player moves beyond a configured threshold.

Note:
- This is a Atrapacielos specialization.
- Auctoritas uses flat addressing for bounded worlds (§10.1).


### 10.3 One implementation, multiple instantiations (zero overhead)
- `SpatialIndexT<Storage>` template,
- `FlatStorage` and `SparseStorage`,
- no virtual dispatch.

---

## 11. Streaming residency state machine (Auctoritas)

Auctoritas streaming requires explicit chunk states:

`Unloaded → Loading → Loaded → Unloading → Unloaded`

Contract:
- `queryFast()` on non-Loaded chunks returns **empty** (never blocks).
- `queryDeterministic()` on non-Loaded chunks is **an error in simulation phase**:
  - Debug: assert + log critical.
  - Profile/Release: deterministic fail-fast (log critical + terminate).
- Write attempts targeting non-Loaded chunks are rejected (never block).
- Mutations require `Loaded` state; streaming must ensure required chunks are loaded before a simulation step that needs them.

State transitions are driven by the streaming manager in budgeted phases.

---

## 12. TileMap chunking (Auctoritas-only) with sub-pages

### 12.1 Coordinate contract
- `worldChunkSizeTiles = 128`,
- `tileSizePx = 256`,
- `worldChunkSizePx = 32768`.

### 12.2 Draw unit vs update unit
- draw unit: world chunk (128×128),
- update unit: sub-pages (32×32 or 64×64) compute dirty vertex ranges.

Contract:
- one GPU buffer per layer per chunk,
- sub-pages do not own GPU buffers,
- partial updates use `VertexBuffer::update()`.

### 12.3 Contiguous sub-page layout
Sub-pages map to contiguous vertex ranges with deterministic offsets; no scatter uploads.

### 12.4 TileMap dense query contract (Auctoritas)
- TileMap provides `forEachTileInRect(Aabb2i rect, Fn fn)` (or equivalent) for dense tile iteration.
- This is independent of SpatialIndex (no entity overhead).
- SpatialIndex remains entity-only broad-phase.
- AI/visibility/selection for tiles uses TileMap API, not SpatialIndex.

---

## 13. Texture scaling to 100k+ — future-proof vertex format

Renderer-internal vertex format includes:
- position (2D),
- uv (2D),
- `pageIndex`,
- `layerIndex`,
- color/tint.

Atrapacielos uses atlas path (`layerIndex = 0`) without Auctoritas shaders.  
Auctoritas may adopt TextureArrays later without refactoring mesh builders.

---

## 14. Zoom LOD & proxy rendering (Auctoritas-only)

Proxy per world chunk for LOD1/LOD2:
- baked terrain texture,
- simplified city/ownership overlay,
- optional low-poly mesh.

Proxy invalidates on tile edits; builds are budgeted/time-sliced; switching is zoom-threshold driven.

---

## 15. Navigation clustering (HPA*) — interface hook

World chunk size is a streaming/coordinate contract. Navigation clustering may differ.

Provide an interface to subdivide a world chunk into nav sectors later; do not bake sector == chunk.

---

## 16. Chunk data IO (Auctoritas-only): non-blocking, mapping-capable backend

Contract:
- versioned, endian-defined binary chunk payloads,
- non-blocking streaming (no synchronous disk reads in simulation/render frame),
- baseline backend on desktop: OS file mapping (mmap / Win32 mapping).
- on platforms where mapping is unavailable, an equivalent non-blocking, budgeted async/zero-copy strategy must provide comparable performance characteristics.

Auctoritas requires the performance characteristics; exact backend is platform-dependent.

---

## 17. Cross-system invalidation hooks (Auctoritas)

Contract: engine-level event/hook mechanism (minimal) for:
- tile edits → invalidate TileMap sub-page geometry,
- tile edits → invalidate chunk proxies,
- tile edits → invalidate nav caches,
- entity bounds changes → mark entity spatial dirty,
- stack membership changes → mark affected entities dirty (Auctoritas gameplay decision).

Hot-path prohibition:
- hooks must not fire inside `queryFast()` / `queryDeterministic()` (i.e., `query*()`) or render hot loops,
- dispatch occurs in dedicated phases (post-sim, pre-render, streaming update).

No cyclic dependencies between systems.

---

## 18. Determinism + ML inference (future constraint)

If ML inference is introduced:
- GPU inference is frequently non-deterministic.

Policy:
- CPU-only backend for deterministic replays, OR
- ML outputs are advice and pass through a deterministic filter/selector.

No ML runtime is required now.

---

## 19. Migration plan (v1 → v2) without breaking everything

1) Implement `SpatialIndexV2` alongside current V1.
2) Add Debug/Profile comparison harness:
   - register same entities,
   - run same queries,
   - compare results (set equality),
   - measure timings.
3) Switch default to V2 (Profile → Debug → Release).
4) Delete V1 after confidence.

No big bang.

---

## 20. Harness requirements (extended)

Harness must report:
- query time distribution (avg/p95/p99),
- dirty update time distribution (avg/p95/p99),
- candidate counts per query (avg/p95/p99),
- overflow metrics:
  - percent of cell visits hitting overflow,
  - overflow elements visited per query,
  - overflow pool pressure (peak allocations, freelist depth),
  - worst hot spots (max density cells),
  - exhaustion tests and failure-mode validation.

Auctoritas determinism checks (Debug/Profile):
- deterministic query mode returns identical ordered results across runs for identical seeds/state,
- deterministic update order is enforced (dirty set sorted by StableID).

---

## 21. Memory ceilings (Auctoritas)

Auctoritas must define explicit ceilings and enforce them with metrics/logging:
- max resident spatial chunks (by state),
- max overflow pool elements / total overflow memory,
- max entities indexed per SpatialIndex instance.

Ceilings are tunable but must exist; exceeding ceilings must trigger the configured failure policy.

---

## 22. Starting defaults (non-contract, dev/profile only)

```cpp
SpatialIndex::Config atrapacielosDefaults {
    .chunkSizeTiles = 0,
    .chunkSizeWorld = 4096,
    .cellSizeWorld = 512,
    .useFlatArray = false,
    .maxActiveChunks = 16
};

SpatialIndex::Config AuctoritasDefaults {
    .chunkSizeTiles = 128,
    .chunkSizeWorld = 32768,
    .cellSizeWorld = 512,
    .useFlatArray = true,
    .maxActiveChunks = 0
};
```

---

## 23. Summary decisions

- Unified `ChunkCoord` in core/spatial.
- SpatialIndex v2: chunked storage + dense cells; no per-cell hashing; no query allocations.
- `Cell`: single cache line, cache-line aligned (`kInlineCapacity = 14`, `sizeof(Cell) == 64`, `alignas(64)`).
- Two query modes: fast unordered (render) + deterministic (Auctoritas simulation).
- Deterministic update order in Auctoritas (dirty set sorted by StableID).
- Phase-based read/write model; future parallelism is enabled by chunk partitioning + barriers/snapshots.
- Overflow pool is deterministic, bounded, and has explicit exhaustion behavior; never allocates in query.
- Duplicate-free results via stamping-based dedup (allocation-free).
- Entity lifetime: destruction is deferred until end-of-phase/tick barrier (mandatory).
- Auctoritas streaming has explicit residency states;
  - `queryFast()` on non-Loaded returns empty,
  - `queryDeterministic()` on non-Loaded is a simulation error (Debug assert, Profile/Release fail-fast).
- Variant C: 128×128 world chunks + TileMap sub-pages for partial updates with contiguous ranges.
- Auctoritas: flat chunk addressing with payload pooling.
- Atrapacielos: bounded-capacity sparse/ring storage.
- Internal vertex format carries `pageIndex/layerIndex`; Atrapacielos can ignore; Auctoritas can adopt TextureArrays later.
- Zoom-out proxy rendering is required for Auctoritas; interface now, implementation later.
- Auctoritas chunk IO is non-blocking and mapping-capable (or equivalent).
- Cross-system invalidations are hook-driven, phase-dispatched, and hot-path forbidden.