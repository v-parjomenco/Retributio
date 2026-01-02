# EnTT Future Optimizations

## Status: Migration Complete ✅

**Current Performance:**
- 500,000 entities @ 277 FPS (Release x64)
- Spatial index + culling working efficiently
- Zero-overhead EnTT integration

**Conclusion:** No immediate optimizations needed. Current architecture ready for 4X production.

---

## Future Enhancement: System Metadata (Year 1+)

**When:** Only if implementing parallel ECS update (multi-threaded systems)  
**Effort:** 2-3 days  
**Priority:** Low (current single-threaded performance is sufficient)

### Goal

Enable safe parallel execution of systems by declaring component dependencies.

### Implementation

```cpp
// core/ecs/system.h
class ISystem {
public:
    virtual ~ISystem() = default;
    
    // Metadata for future job scheduler (optional, defaults to empty)
    virtual std::span<const std::type_index> getReadComponents() const { return {}; }
    virtual std::span<const std::type_index> getWriteComponents() const { return {}; }
    
    virtual void update(World& world, float dt) { /* ... */ }
    virtual void render(World& world, sf::RenderWindow& window) { /* ... */ }
};
```

### Example

```cpp
// MovementSystem: reads Velocity, writes Transform + SpatialDirtyTag
std::span<const std::type_index> MovementSystem::getReadComponents() const {
    static const std::type_index deps[] = { typeid(VelocityComponent) };
    return deps;
}

std::span<const std::type_index> MovementSystem::getWriteComponents() const {
    static const std::type_index deps[] = { 
        typeid(TransformComponent), 
        typeid(SpatialDirtyTag) 
    };
    return deps;
}
```

### Job Graph Scheduler (Future)

```cpp
// SystemManager builds dependency graph from metadata
// Systems with disjoint write sets → parallel execution
// Systems with overlapping writes → sequential execution
```

### Benefits

- Safe parallelization without data races
- Self-documenting component access patterns
- Foundation for multi-threaded ECS

### Important

- Only implement if profiling shows single-threaded bottleneck
- Current 277 FPS @ 500K entities suggests no need until 1M+ entities
- YAGNI principle applies

---

## Other Potential Enhancements (Low Priority)

### 1. Sparse Components (when needed)

**When:** If >50% entities don't need a component but it's checked frequently  
**Example:** Diplomacy data (only faction leaders need it)  
**How:** EnTT supports this natively, just use carefully

### 2. Hierarchical LOD Systems (4X-specific)

**When:** Strategic layer with 1000+ AI empires  
**How:** Different update frequencies for strategic/operational/tactical layers  
**Benefit:** AI time-slicing, performance budgets

### 3. Snapshot/Replay (EnTT native)

**When:** Save/load or deterministic replay needed  
**How:** Use EnTT's `snapshot()` API  
**Note:** Requires stable entity ID mapping for persistence

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

---

## Conclusion

Current EnTT architecture is production-ready and over-engineered for current scale. Future optimizations should be driven by:

1. **Measured bottlenecks** (profile first)
2. **Actual production requirements** (not theoretical max)
3. **YAGNI principle** (implement when needed, not before)

**Recommendation:** Focus on game features. Revisit performance when/if needed.