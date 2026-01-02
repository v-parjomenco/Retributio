# ENGINEERING ROADMAP: "Titan" 4X Strategy Engine

**Target Scale:** 1,000,000+ Entities | 50,000+ Unique Textures | Neural AI  
**Target Hardware:** Ryzen 9 7945HX (32 Threads) | 32GB RAM | Discrete GPU  
**Tech Stack:** C++20 | EnTT 3.16 | SFML 3.0.2 (OpenGL backend)

---

## 1. Global Architecture Strategy (The "Job System")

**Philosophy:** Move away from "Systems update huge lists sequentially" to "Systems generate parallel tasks."

### Phase 2.1: Task-Based Parallelism
**WHEN:** Start of 4X Development  
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
**WHEN:** Early 4X Prototype  
**WHY:** Standard sprite batching **fails** with 50k unique textures. Switching textures 50k times/frame destroys performance regardless of CPU power.

*   **Problem:** `sf::Texture::bind()` is expensive. 50k binds = <5 FPS.
*   **Solution: Array Textures (OpenGL Layer):**
    *   Load icons of similar size (e.g., 64x64 units) into a single OpenGL **Texture Array** (layers).
    *   **Custom Shader:** Write a custom fragment shader that accepts `layer_id` as a vertex attribute.
    *   **Result:** Draw 100,000 units with *different* textures in **1 draw call**.
*   **Fallback:** Mega-Atlases (8192x8192) with dynamic packing (slower, but standard SFML).

### Phase 2.3: Static Geometry Chunking
**WHEN:** Mid 4X Development  
**WHY:** Terrain/Map tiles (500k+) do not move.

*   **Strategy:** Bake terrain vertices into `sf::VertexBuffer` chunks (e.g., 64x64 tile sectors).
*   **Update:** Only rebuild the specific chunk vertex buffer when terrain changes (terraforming/nukes).
*   **Target:** <1ms CPU cost for background rendering.

### Phase 2.4: Rendering LOD (Level of Detail)
**WHEN:** Mid 4X Development  
**WHY:** Pixel density limits. Drawing 1M sprites on a 4K screen is wasteful.

*   **L0 (Close):** High-res Sprite (Texture Array).
*   **L1 (Mid):** Low-res Icon / Dot.
*   **L2 (Far):** Density Map / Heatmap (Aggregated rendering via shader).

---

## 3. Simulation & AI Roadmap (Scientific/Neural)

### Phase 3.1: AI Level of Detail (AI-LOD)
**WHEN:** Early AI implementation  
**WHY:** Running a Neural Net for 1M entities takes minutes per turn.

*   **Tier 1 (Focus):** Leaders/Generals (~1k entities). Full Neural Net inference.
*   **Tier 2 (Active):** Local Commanders (~50k entities). Heuristic/Behavior Tree.
*   **Tier 3 (Background):** Civilians/Pop (~950k entities). Statistical simulation / Dice rolls (O(1)).

### Phase 3.2: Batch Inference System
**WHEN:** Integration of Neural Networks (ONNX Runtime / Torch)  
**WHY:** Inferencing 1 by 1 is slow due to CPU/RAM latency.

*   **Architecture:** `NeuralBrainComponent` stores inputs/weights.
*   **System:** `AIInferenceSystem` gathers inputs from 1000 entities into a single **Tensor Batch**.
*   **Execution:** Send batch to ONNX Runtime (uses AVX-512 on Ryzen 9).

---

## 4. Data & Memory Roadmap (The "History Simulator")

### Phase 4.1: Component Compression & SoA
**WHEN:** Continuous Optimization  
**WHY:** 1M entities with complex data = High RAM usage. Cache misses kill performance.

*   **SoA (Structure of Arrays):** Crucial for "Scientific" simulation.
    *   *Bad:* `struct Unit { float health; float morale; vec2 pos; ... }` (AoS)
    *   *Good:* EnTT naturally handles this, but component design must be granular. 
    *   *Optimization:* Use `std::uint8_t` or bitfields where possible.

### Phase 4.2: Historical Branching (Delta Snapshots)
**WHEN:** Core Mechanics Implementation  
**WHY:** Storing full world copies for "Alternate Histories" will exhaust 32GB RAM instantly (1M entities * 500 bytes * 100 turns = 50GB).

*   **Strategy: Copy-on-Write / Delta Trees.**
*   **Implementation:**
    *   Base State (Turn 0).
    *   Turn N: Store *only* modified components (Delta).
    *   Branching: A new chain of Deltas referencing the parent state.
*   **Serialization:** Use binary serialization (e.g., `zpp_bits` or custom) for disk dumping when RAM fills up.

---

## 5. Hardware Reality Check & Testing

### RAM Constraint (32GB)
*   **Risk:** High.
*   **Mitigation:** 
    1. Aggressive component reuse (Flyweight pattern for static data like Unit Stats).
    2. Stream historical data to SSD (NVMe) if looking back >10 turns.

### CPU Throughput (Ryzen 9 7945HX)
*   **Capacity:** Extreme.
*   **Requirement:** Your code *must* be lock-free. Using `std::mutex` in the hot loop will kill the advantage of 32 threads. Use atomic flags or independent memory ranges.

---

## 6. Success Criteria (Updated)

### Milestone: SkyGuard (Baseline)
- [x] 500k entities simulated (Background).
- [x] 50k visible sprites @ 60 FPS (Single Threaded).
- [ ] **Architecture:** Systems strictly separated from Data (Pure ECS).

### Milestone: 4X "Titan" Prototype
- [ ] **Rendering:** Texture Arrays implemented. 50k *unique* textures rendering @ 60 FPS.
- [ ] **Job System:** Simulation loop utilizing 30+ threads.
- [ ] **AI:** Batch inference running on <5s per turn for 10k agents.
- [ ] **Memory:** 1M entities fit in <4GB RAM (Current State).

### Milestone: Production
- [ ] **History:** Instant switching between timeline branches (Delta application).
- [ ] **Simulation:** Full turn processing (1M entities) < 10 seconds.