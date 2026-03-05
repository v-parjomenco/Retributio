# Retributio / Atrapacielos — RoadMap Items
# Features planned for future implementation, not current requirements

## КОГДА: При достижении Atrapacielos stress test 50K+ entities
- **Custom Allocators Implementation**
  - Pool allocators для ECS components (100K+ entities)
  - Arena allocators для per-frame data
  - Memory budgets tracked via metrics
  - Детали: конкретные библиотеки, implementation strategy

## КОГДА: С момента начала разработки 4X (flagship game)
- **Component Versioning System**
  - Per-component version (static constexpr uint32_t kVersion)
  - Migration functions для version bumps
  - Loader поддержка N-1, N-2 backward compatibility
  - Детали: migration tooling, version bump workflow

- **Stable Entity ID Mapping**
  - Separate uint64 stable IDs for saves/network
  - Mapping table: entt::entity ↔ StableID
  - Never serialize entt::entity directly
  - Детали: ID allocation strategy, persistence format

- **Save System (Large-Scale)**
  - Delta saves (only changed components since last save)
  - Compressed sparse serialization
  - Chunked loading/streaming (entity→chunk mapping)
  - Save metadata: entity counts, playtime, game-state hash
  - Детали: compression algorithms, chunk size tuning

- **Deterministic Math Libraries**
  - Fixed-point math для combat/economy calculations
  - Cross-platform sin/cos lookup tables
  - Consistent FP mode enforcement
  - Детали: конкретные библиотеки (e.g., fpm, libfixmath)

## КОГДА: Когда профилирование покажет allocation bottleneck
- **Custom Allocator Tuning**
  - Specific pool sizes
  - Arena size configuration
  - Memory pressure policies
  - Детали: profiling results analysis, tuning parameters

## КОГДА: AI system достигнет baseline functionality
- **AI Budget Tuning**
  - Конкретные millisecond budgets:
    - Strategic AI: ~100ms per empire per strategic update (every 10 turns)
    - Operational AI: ~50ms per front per operational update (every 3 turns)
    - Tactical AI: ~5ms per active unit group per turn
  - Time-slicing resume points
  - Budget enforcement mechanisms
  - Детали: measurements, adjustment based on target hardware

## КОГДА: После core gameplay loop завершен
- **Event System (Pub-Sub)**
  - Event bus for cross-system communication
  - Event history timeline (queryable)
  - Event triggers and effects (scriptable)
  - Детали: event schema, trigger language design

- **Scripting Language Layer**
  - Lua или custom scripting для complex event logic
  - Script API design (bindings to engine)
  - Modding support через scripts
  - Детали: language choice, API surface, sandboxing

## КОГДА: Content iteration стал bottleneck
- **Hot-Reload System**
  - File watching для JSON configs (engine/gameplay/AI)
  - Live reload without restart
  - Validation на reload
  - Детали: file watcher implementation, validation strategy

## КОГДА: 4X map достигнет 10K+ provinces
- **Chunked Maps / LOD System**
  - core::spatial chunked storage
  - LOD for distant entities (skip AI, reduce detail)
  - Streaming для large worlds
  - Детали: chunk size, LOD levels, streaming thresholds

## КОГДА: Multiplayer стал priority
- **Multiplayer Netcode**
  - Deterministic simulation enforcement (strict)
  - Lockstep multiplayer architecture
  - Checksum validation on turn boundaries
  - Rollback/resync mechanisms
  - Детали: network protocol, desync handling, bandwidth optimization

- **Float Determinism Strict Mode**
  - Platform-agnostic math (no std::sin/atan2)
  - Fixed-point arithmetic enforcement для critical paths
  - Cross-platform validation tests
  - Детали: math library integration, validation test suite

## КОГДА: Performance critical features нужны раньше
- **Parallel Job System**
  - Thread pool + task scheduler
  - System dependencies автоматическое определение (from component access declarations)
  - Phase-based parallel execution
  - Детали: job system library choice, scheduling algorithm

## КОГДА: Tools development начинается
- **Tooling Ecosystem**
  - Map editor
  - Event/AI script editor
  - Save file inspector/debugger
  - Profiling/metrics dashboard
  - Детали: tool architecture, integration points

---

## SUMMARY: Timeline Buckets

**BUCKET 1 — Atrapacielos Stress Test (50K entities):**
- Custom allocators profiling + potential implementation

**BUCKET 2 — 4X Development Start:**
- Component versioning
- Stable entity IDs
- Save system (delta/compressed/chunked)
- Deterministic math libraries (fixed-point)

**BUCKET 3 — AI Baseline Complete:**
- AI budget tuning (specific millisecond values)

**BUCKET 4 — Core Gameplay Loop Complete:**
- Event system (pub-sub, history, scriptable)
- Scripting language layer

**BUCKET 5 — Content Iteration Bottleneck:**
- Hot-reload system

**BUCKET 6 — Large-Scale 4X Maps (10K+ provinces):**
- Chunked maps / LOD system

**BUCKET 7 — Multiplayer Priority:**
- Multiplayer netcode
- Float determinism strict mode enforcement

**BUCKET 8 — Performance-Critical Features:**
- Parallel job system (may be earlier if profiling shows need)

**BUCKET 9 — Tools Development:**
- Tooling ecosystem (editors, debuggers, dashboards)

---

## NOTES:
- Эти items НЕ входят в current requirements guide
- Детали implementation должны документироваться отдельно когда начинается работа
- Приоритеты могут измениться based on profiling/user feedback
- Некоторые features могут быть pulled forward если станут блокерами