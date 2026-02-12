# Stress Modes (SkyGuard now, Titan later)

## ActiveSet (implemented)

- **Tests:** residency/streaming correctness, spatial index capacity and update path, memory pressure, deterministic stability checks.
- **Does NOT test:** render pipeline throughput as a primary KPI.
- **Primary KPI:** active-set ceilings, registration/update latency, determinism stability, overflow pressure.

## Render (planned)

- **Tests:** render gather/sort/build/draw throughput, batching efficiency, texture switch pressure.
- **Primary KPI:** frame CPU breakdown (`gather/sort/build/draw`), draw-calls vs visible sprites, texture switch cost.

## Query (planned)

- **Tests:** spatial query latency and candidate behavior across different AABB patterns/sizes.
- **Primary KPI:** query latency p50/p95/p99, `entriesScanned/uniqueAdded`, duplicate-hit ratio.

## Sim (future)

- **Tests:** tick/turn budget under deterministic synthetic workloads once 4X simulation loops exist.
- **Primary KPI:** tick duration and phase budgets under write/read barriers.

## Cross-mode rules

1. Reproducibility first: fixed seeds and named presets.
2. No mixed KPI interpretation: each mode has one primary objective.
3. Measure before optimize: only optimize after baseline capture.
4. Keep Release hot path clean: stress diagnostics remain Debug/Profile only.