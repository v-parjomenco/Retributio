# ENGINEERING ROADMAP: "Titan" 4X Strategy Engine

**Target Scale:**        1,000,000+ Entities     | 100,000+ Unique Textures | Neural AI
**Target Hardware:** Ryzen 9 7945HX (32 Threads) | 32GB RAM                 | Discrete GPU
**Tech Stack:**          C++20                   | EnTT 3.16                | SFML 3.0.2 (OpenGL backend)

**NOTE:** This document details rendering/performance strategies. See `SFML1_SkyGuard_RoadMap_v2_2_FINAL.md` for master timeline.

---

## 1. Global Architecture Strategy (The "Job System")

**Philosophy:** Move away from "Systems update huge lists sequentially" to "Systems generate parallel tasks."

### Phase 2.1: Task-Based Parallelism
**WHEN:** Month 12 (Headless Server Build, job system foundation)
**WHY:** Single-threaded iteration over 1M entities takes too long (>100ms). 32 threads must be saturated.

*   **Integration:** Integrate a task system (e.g., `taskflow` or `std::jthread` pool) alongside EnTT.
*   **Sharding:** Logic systems split Entity ID ranges into chunks (e.g., 10k entities per chunk).
*   **Execution:**
    *   Main Thread: Rendering (SFML), Input, Dispatch.
    *   Worker Threads (31): Simulation, Pathfinding, AI Inference.
*   **EnTT Safety:**
    *   *Reads:* Safe parallel access via `view<const T>`.
    *   *Writes:* Deferred command buffers or strictly isolated chunks (no two threads write to the same component type on the same entity).

---

## 2. Rendering Performance Roadmap

### Phase 2.2: Advanced Texture Management (CRITICAL)
**WHEN:** Month 7-8 (Texture System implementation)
**WHY:** Standard sprite batching **fails** with 100K unique textures. Switching textures 100K times/frame destroys performance regardless of CPU power.

*   **Problem:** `sf::Texture::bind()` is expensive. 50k binds = <5 FPS.

*   **PRIMARY SOLUTION: Multi-Atlas System (8K x 8K atlases):**
    *   **Strategy:** Category-based atlases for different asset types.
    *   **Categories:**
        *   Buildings: 16K textures (128x128 grid @ 64x64 tiles)
        *   Units: 32K textures (needs 2 atlases or larger grid)
        *   Techs: 16K textures
        *   Terrain: 16K textures
        *   Leaders: 8K textures
        *   UI: 8K textures
    *   **Total Capacity:** 4-8 atlases = 96K-128K unique textures.
    *   **Performance:** Batching by category = 4-8 draw calls per frame (acceptable).
    *   **Implementation:** Standard SFML rendering (no custom shaders required).
    *   **Proven:** Used by Civilization III and similar AAA 4X games.
    *   **Advantages:** Reliable, hardware-agnostic, no driver dependencies.

*   **OPTIONAL ENHANCEMENT: Texture Arrays (OpenGL Layer):**
    *   **Strategy:** Load icons of similar size (e.g., 64x64 units) into a single OpenGL **Texture Array** (layers).
    *   **Custom Shader:** Write a custom fragment shader that accepts `layer_id` as a vertex attribute.
    *   **Result:** Draw 100,000 units with *different* textures in **1 draw call** (faster than multi-atlas).
    *   **Requirements:**
        *   GPU must support 4K+ layers (query `GL_MAX_ARRAY_TEXTURE_LAYERS`).
        *   Intel integrated often limited to 2048 layers (insufficient for full scale).
        *   Custom OpenGL shader integration with SFML.
    *   **CRITICAL:** ALWAYS implement fallback to multi-atlas if:
        *   Texture arrays unsupported by hardware (old Intel GPUs).
        *   Driver bugs detected (especially Intel UHD 630).
        *   Max layers < 2048 (query at runtime).
    *   **Status:** Experimental enhancement, NOT required for 100K textures.
    *   **Testing:** Validate on Intel UHD 630 before relying on arrays (see GPU_Testing_Guide.md).

### Phase 2.3: Static Geometry Chunking
**WHEN:** Month 24 (Performance Final Pass)
**WHY:** Terrain/Map tiles (500k+) do not move.

*   **Strategy:** Bake terrain vertices into `sf::VertexBuffer` chunks (e.g., 64x64 tile sectors).
*   **Update:** Only rebuild the specific chunk vertex buffer when terrain changes (terraforming/nukes).
*   **Target:** <1ms CPU cost for background rendering.

### Phase 2.4: Rendering LOD (Level of Detail)
**WHEN:** Month 24 (Performance Final Pass, if needed)
**WHY:** Pixel density limits. Drawing 1M sprites on a 4K screen is wasteful.

*   **L0 (Close):** High-res Sprite (Multi-Atlas or Texture Array).
*   **L1 (Mid):** Low-res Icon / Dot (single pixel).
*   **L2 (Far):** Density Map / Heatmap (Aggregated rendering via shader).

---

## 3. Simulation & AI Roadmap (Scientific/Neural)

### Phase 3.1: AI Level of Detail (AI-LOD)
**WHEN:** Month 18 (AI Time-Slicing)
**WHY:** Running AI logic for 1M entities every tick is impossible.

*   **Tier 1 (Focus):** Leaders/Generals (~1k entities). Full AI logic (heuristic or neural).
*   **Tier 2 (Active):** Local Commanders (~50k entities). Simplified heuristic/Behavior Tree.
*   **Tier 3 (Background):** Civilians/Population (~950k entities). Statistical simulation / Dice rolls (O(1)).

### Phase 3.2: Batch Inference System (Neural AI)
**WHEN:** Year 2 Q1-Q2 (Neural AI Integration, optional)
**WHY:** Inferencing 1 by 1 is slow due to CPU/RAM latency.

*   **Architecture:** `NeuralBrainComponent` stores inputs/weights.
*   **System:** `AIInferenceSystem` gathers inputs from 1000 entities into a single **Tensor Batch**.
*   **Execution:** Send batch to ONNX Runtime (uses AVX-512 on Ryzen 9).
*   **Fallback:** Heuristic AI ALWAYS available (neural = optional enhancement).

---

## 4. Data & Memory Roadmap (The "History Simulator")

### Phase 4.1: Component Compression & SoA
**WHEN:** Month 23 (Memory Optimization Pass)
**WHY:** 1M entities with complex data = High RAM usage. Cache misses kill performance.

*   **SoA (Structure of Arrays):** Crucial for "Scientific" simulation.
    *   *Bad:* `struct Unit { float health; float morale; vec2 pos; ... }` (AoS)
    *   *Good:* EnTT naturally handles this, but component design must be granular.
    *   *Optimization:* Use `std::uint8_t` or bitfields where possible.
*   **Component Size Audit:**
    *   Hot components: <64 bytes (cache-line friendly)
    *   Cold components: <256 bytes (rarely accessed)
*   **Target:** 1M entities @ <4GB RAM (current state only, not history).

### Phase 4.2: Historical Branching (Delta Snapshots)
**WHEN:** Year 2+ (Chunk-based save system)
**WHY:** Storing full world copies for "Alternate Histories" will exhaust 32GB RAM instantly (1M entities * 500 bytes * 100 turns = 50GB).

*   **Strategy: Copy-on-Write / Delta Trees.**
*   **Implementation:**
    *   Base State (Turn 0).
    *   Turn N: Store *only* modified components (Delta).
    *   Branching: A new chain of Deltas referencing the parent state.
*   **Serialization:** Use binary serialization (EnTT snapshot + compression) for disk dumping when RAM fills up.

---

## 5. Hardware Reality Check & Testing

### RAM Constraint (32GB)
*   **Risk:** High for historical branching (100+ turns * 1M entities).
*   **Mitigation:**
    1. Aggressive component reuse (Flyweight pattern for static data like Unit Stats).
    2. Stream historical data to SSD (NVMe) if looking back >10 turns.
    3. Delta compression (only store changed components).

### CPU Throughput (Ryzen 9 7945HX)
*   **Capacity:** Extreme (32 threads).
*   **Requirement:** Your code *must* be lock-free. Using `std::mutex` in the hot loop will kill the advantage of 32 threads. Use atomic flags or independent memory ranges.
*   **Target:** 30+ threads saturated during simulation phase.

---

## 6. Success Criteria (Updated)

### Milestone: SkyGuard (Baseline)
- [x] 500k entities simulated (Background).
- [x] 50k visible sprites @ 60 FPS (Single Threaded).
- [x] **Architecture:** Systems strictly separated from Data (Pure ECS).

### Milestone: 4X "Titan" Prototype (Month 24)
- [ ] **Rendering:** Multi-atlas system implemented. 100K *unique* textures rendering @ 144 FPS.
- [ ] **Job System:** Simulation loop utilizing 30+ threads.
- [ ] **AI:** Heuristic AI baseline running <5s per turn for 20 civs.
- [ ] **Memory:** 1M entities fit in <4GB RAM (Current State).

### Milestone: Production (Year 2+)
- [ ] **Rendering:** Texture arrays (optional) OR multi-atlas performing @ 144 FPS.
- [ ] **History:** Instant switching between timeline branches (Delta application).
- [ ] **Simulation:** Full turn processing (1M entities) < 10 seconds.
- [ ] **Neural AI (optional):** Batch inference <10s per turn for 1000 leaders.

---

## 7. Key Architectural Decisions (Aligned with Canon)

### Texture System Priority
**Decision:** Multi-atlas PRIMARY, texture arrays OPTIONAL.
**Rationale:**
- Multi-atlas proven (Civ III) and hardware-agnostic.
- Texture arrays experimental, Intel compatibility issues.
- Fallback ALWAYS available = robust production.

### Parallelism Timing
**Decision:** Design for parallelism (Week 1), implement when needed (Month 12).
**Rationale:**
- System metadata (component access) NOW = easy parallelization LATER.
- Job system only if profiling shows FPS < 30 (likely Month 12 for 1M entities).

### Neural AI Timing
**Decision:** Heuristic baseline (Month 10-11), neural OPTIONAL (Year 2+).
**Rationale:**
- Heuristic AI = 80% quality, fast implementation.
- Neural AI = experimental enhancement, NOT gameplay blocker.

---

**END OF ROADMAP**

See `SFML1_SkyGuard_RoadMap_v2_2_FINAL.md` for complete master timeline and all phases.
