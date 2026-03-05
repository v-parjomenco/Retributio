# EnTT Future Optimizations

**Current Performance:**
- 500,000 entities @ 277 FPS (Release x64)
- Spatial index + culling working efficiently
- Zero-overhead EnTT integration

**NOTE:** This document covers EnTT-specific optimizations. See `retributio_atrapacielos_RoadMap_v2_2_FINAL.md` for master timeline.

---

## System Metadata

**Status:** Implemented in Week 1 (Phase 1)
**Location:** `core/ecs/system.h`
**Purpose:** Enable safe parallel execution of systems by declaring component dependencies.

### Implementation

```cpp
// core/ecs/system.h
class ISystem {
public:
    virtual ~ISystem() = default;
    
    // Component access metadata
    virtual std::span<const ComponentID> getReadComponents() const noexcept { return {}; }
    virtual std::span<const ComponentID> getWriteComponents() const noexcept { return {}; }
    
    // Update frequency (multi-frequency support)
    virtual std::uint32_t getFrequency() const { return 1; }
    
    // Update phase (deterministic ordering)
    virtual UpdatePhase getPhase() const = 0;
    
    virtual void update(World& world, float dt) { /* ... */ }
    virtual void render(World& world, sf::RenderWindow& window) { /* ... */ }
};
```

### Example

```cpp
// MovementSystem: reads Velocity, writes Transform + SpatialDirtyTag
std::span<const ComponentID> MovementSystem::getReadComponents() const noexcept {
    static const ComponentID deps[] = { entt::type_hash<VelocityComponent>::value() };
    return deps;
}

std::span<const ComponentID> MovementSystem::getWriteComponents() const noexcept {
    static const ComponentID deps[] = { 
        entt::type_hash<TransformComponent>::value(), 
        entt::type_hash<SpatialDirtyTag>::value()
    };
    return deps;
}
```

### Job Graph Scheduler (Future)

**WHEN:** Month 12 (Job System Foundation), if profiling shows FPS < 30

```cpp
// SystemManager builds dependency graph from metadata
// Systems with disjoint write sets → parallel execution
// Systems with overlapping writes → sequential execution
```

### Benefits

- Safe parallelization without data races
- Self-documenting component access patterns
- Foundation for multi-threaded ECS

### Current Status

- ✅ Metadata API implemented (Week 1)
- ⏳ Job scheduler implementation: Month 12 (if needed)
- Current 277 FPS @ 500K entities suggests no urgency until 1M+ entities

---

## Other Potential Enhancements (Low Priority)

### 1. Sparse Components (when needed)

**When:** If >50% entities don't need a component but it's checked frequently
**Example:** Diplomacy data (only faction leaders need it)
**How:** EnTT supports this natively via `sparse_set`, just use carefully
**Timeline:** Month 23 (Memory Optimization Pass), only if profiling shows benefit

### 2. Hierarchical LOD Systems (4X-specific)

**When:** Strategic layer with 1000+ AI empires (Year 2+)
**How:** Different update frequencies for strategic/operational/tactical layers
**Benefit:** AI time-slicing, performance budgets
**Timeline:**
- Month 18: AI Time-Slicing (basic multi-frequency)
- Year 2+: Advanced LOD with statistical simulation for background entities

### 3. Snapshot/Replay (EnTT native)

**When:** Save/load or deterministic replay needed
**How:** Use EnTT's `snapshot()` API
**Note:** Requires stable entity ID mapping for persistence (Month 9)
**Timeline:**
- Month 8: Binary Serialization System (EnTT snapshot)
- Month 9: Stable Entity IDs
- Month 10-11: Save/Load MVP

---

## Performance Monitoring

**Watch for:**
- FPS drops below 60 at target entity count
- Memory usage growing non-linearly
- Spatial index performance degradation

**Current baseline (500K entities):**
- FPS: 277
- CPU: 5.0 ms/frame
- Draw calls: ~200
- Culling: efficient (16K visible / 375K total)

**Action trigger:** If performance drops 50% at production scale, revisit optimizations.

**Target (1M entities):**
- FPS: 60 minimum (144 target)
- CPU: <16.6 ms/frame (60 FPS) or <6.9 ms/frame (144 FPS)
- Draw calls: <10 (multi-atlas batching)

---

## Implementation Timeline (Aligned with Canon)

### Week 1 (Phase 1)
- [ ] System Metadata API (component access declarations)
- [ ] Phase-based scheduler (Input → Simulation → AI → Resolution → Render)
- [ ] Multi-frequency support (`getFrequency()`)

### Month 8-9 (Phase 4)
- [ ] Binary Serialization (EnTT snapshot)
- [ ] Stable Entity IDs (separate from entt::entity)

### Month 12 (Phase 4)
- [ ] Job System Foundation (if profiling shows FPS < 30)
  - Use metadata to build dependency graph
  - Parallel system execution (30+ threads)

### Month 18 (Phase 5)
- [ ] AI Time-Slicing (multi-frequency for strategic/operational/tactical)

### Month 23 (Phase 5)
- [ ] Memory Optimization Pass
  - Sparse component analysis
  - Component size audit (<64 bytes hot, <256 bytes cold)

### Year 2+ (Phase 7)
- [ ] Advanced LOD (statistical simulation for background entities)
- [ ] Chunk-based saves (delta compression, historical branching)

---

## Key Performance Assumptions

### YAGNI Principle Applied
- **Don't optimize prematurely:** Current 277 FPS @ 500K entities is excellent.
- **Profile first:** Only optimize when profiling shows bottleneck.
- **Measure impact:** Every optimization must show >10% improvement in real workload.

### Current Architecture Strengths
- ✅ EnTT view iteration: Cache-friendly, minimal overhead
- ✅ Spatial index: O(log n) queries, efficient culling
- ✅ Phase-based scheduler: Deterministic ordering
- ✅ System metadata: Ready for parallelization

### Known Future Bottlenecks (to address when encountered)
1. **1M entities @ 60 FPS single-threaded:** Unlikely, will need job system (Month 12)
2. **100K textures rendering:** Multi-atlas solves (Month 7-8)
3. **AI computation (1000+ civs):** Time-slicing + LOD solves (Month 18)
4. **Memory (1M entities):** Component compression + sparse analysis (Month 23)

---

## Conclusion

Future optimizations should be driven by:

1. **Measured bottlenecks** (profile first, Tracy integration Week 3-4)
2. **Actual production requirements** (not theoretical max)
3. **YAGNI principle** (implement when needed, not before)

**Current Priority:** Build Atrapacielos (Phase 1-3), validate architecture with real gameplay, THEN optimize based on actual 4X workload (Phase 4+).

---

**END OF DOCUMENT**

See `retributio_atrapacielos_RoadMap_v2_2_FINAL.md` for complete master timeline.
