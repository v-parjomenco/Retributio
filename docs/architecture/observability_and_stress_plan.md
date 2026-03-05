# Observability & Stress Testing Plan

**Engine:** Atrapacielos (tech demo / 4X Auctoritas lab)
**Status:** Pre-CMake, VS .sln, solo/small-team
**Principle:** YAGNI — build only what solves real problems today; scaffold for tomorrow only when the cost is zero.

---

## Текущее состояние (baseline)

### Что работает

- **Debug overlay** — полный набор метрик: FPS, Counts, Render, Textures, Culling, RenderFilter, CPU timings (Profile), SpatialGrid, SpatialQuery, SpatialVisit, Storages, Background, View, CellsHealth, Player, PlayerVis.
- **ActiveSet stress mode** — ENV-driven (`RETRIBUTIO_PROFILE` build), 1M+ entities, deterministic spatial streaming.
- **Epoch-based texture cache** — per-frame O(1) lookups, generation-safe.
- **CappedAppender** — bounded overlay buffers, visible truncation marker, zero realloc in steady-state.
- **Validate-on-write / trust-on-read** — контракт соблюдён сквозь всю цепочку loader → props → system.
- **Adaptive CPU timing** — sub-ms значения отображаются в µs (`123us`), ≥1ms в ms (`1.2ms`).
- **Stress runtime stamp** — overlay строка `Stress: mode= seed= ...` (Profile only), заполняется один раз в `initWorld()`.
- **Launch presets** — `presets/active_small.env`, `presets/active_large.env` с README.
- **Stress modes taxonomy** — `docs/stress-modes.md` (ActiveSet/Render/Query/Sim).
- **Модульный overlay** — debug extra lines вынесены в `dev/overlay_extras.*`, game.cpp — чистый дирижёр.

### Известные проблемы (актуальные)

| # | Проблема | Влияние | Приоритет |
|---|---------|---------|-----------|
| O4 | `predictedVisible` — прогноз, не факт render pipeline | Расхождение = баг, но отсутствие факта усложняет диагностику | P3 |

### Закрытые проблемы

| # | Проблема | Решение | Закрыто |
|---|---------|---------|---------|
| O1 | `CPU: totalMs=0.0` при <1ms | `appendAdaptiveTimingFromUs`: µs/ms автоматически | Phase 1 |
| O2 | `chunksLoaded` — двусмысленное имя | Переименовано в `chunksLoadedVisited` + комментарий | Phase 0 |
| O3 | Нет stress runtime stamp | `formatStressStampLine` + `StressRuntimeStamp` (one-shot init) | Phase 0 |
| O5 | Нет пресетов запуска | `presets/active_small.env`, `active_large.env` + README | Phase 1 |

---

## Фазы

### ~~Фаза 0 — Observability cleanup~~ ✅ DONE

- [x] 0.1 Stress runtime stamp в overlay (Profile only)
- [x] 0.2 `chunksLoaded` → `chunksLoadedVisited`

### ~~Фаза 1 — Observability precision + presets~~ ✅ DONE

- [x] 1.1 Adaptive CPU display (`appendAdaptiveTimingFromUs`)
- [x] 1.2 Launch presets (`presets/active_small.env`, `active_large.env`, README)
- [x] 1.3 Stress mode taxonomy → реализовано как `docs/stress-modes.md`

---

### Фаза 2 — RenderStress mode (P1, до или параллельно с CMake)

**Когда:** Когда ActiveSet stress стабильно работает и метрики читаемы (после Фаз 0-1).
**Цель:** Изолированно стрессовать render pipeline: видимые спрайты, batching, texture switches.

**Почему раньше QueryStress:** Atrapacielos — это 2D sprite renderer. Render pipeline — то, что реально существует и работает. QueryStress имитирует 4X-паттерны, которых ещё нет. YAGNI.

#### 2.1 RenderStress content provider

**Что нужно:**
- Генерация плотного visible set **вокруг камеры** (не разбросанного по миру).
- Контролируемые профили через ENV:
  - `single_tex_dense` — одна текстура, максимум batching → baseline draw throughput.
  - `multi_tex_dense` — N текстур, максимум texture switches → worst-case batching.
  - `z_layer_heavy` — много уникальных zOrder → sort pressure.
  - `mixed_rotation` — все спрайты повёрнуты → build pressure (sin/cos path).

**Ключевое отличие от ActiveSet:** ActiveSet = "держим ли мир?" (1M entities, streaming).
								   RenderStress = "держим ли кадр?" (50k visible, dense near camera).

#### 2.2 Render-specific KPI

Добавить в overlay (только в RenderStress mode):
- `visibleDensity=` (visible sprites / view area)
- `avgSpritesPerBatch=` (sprites / drawCalls)

**Пресеты:**
```
presets/
  render_single_tex.env
  render_multi_tex.env
```

**DoD:** RenderStress запускается пресетом. CPU breakdown показывает ненулевые значения. Профили дают разную нагрузку на разные фазы.

---

### Фаза 3 — CMake migration (отдельный трек)

**Когда:** По готовности (планируется скоро).
**Влияние на observability:** Минимальное. Overlay/stress — это runtime код, не build system.

**После CMake становится возможным:**
- CI runners с фиксированным hardware
- Nightly bench jobs
- Build matrix (Debug/Profile/Release × stress presets)

---

### Фаза 4 — QueryStress mode (P2, после CMake)

**Когда:** Когда появляется хотя бы прототип 4X-модели (selection, perception, events) или когда SpatialIndex будет использоваться для non-render queries.
**Цель:** Смоделировать 4X-паттерны: частые мелкие запросы (перцепция), средние (тактика), редкие крупные (стратегия).

**Почему не раньше:** Без реального 4X-кода неизвестно:
- Какого размера AABB будут запросы?
- Сколько запросов в тик?
- Какое соотношение read/write?

Преждевременная оптимизация query под угаданные паттерны — waste.

#### 4.1 Query profiles

- `many_small` — 1000 запросов с AABB = 1-2 ячейки (юниты проверяют окружение).
- `medium_scan` — 50 запросов с AABB = 1-2 чанка (события, зоны влияния).
- `large_sweep` — 5 запросов с AABB = пол-мира (стратегический обзор).

#### 4.2 Query-specific KPI

- Query latency distribution (p50/p95/p99) per profile.
- `entriesScanned / uniqueAdded` ratio (эффективность индекса).
- `dupHits / entriesScanned` ratio (overhead дедупликации).
- Non-loaded chunk policy: `skippedNonLoaded` должен быть 0 в loaded-world сценарии.

**DoD:** QueryStress запускается пресетом. KPI выводятся в overlay. Baseline зафиксирован.

---

### Фаза 5 — CI/Bench productization (P2, после CMake)

**Когда:** После CMake + хотя бы 2 стабильных stress-режима (ActiveSet + Render).

#### 5.1 Nightly perf jobs

- Каждый пресет запускается на фиксированном runner.
- Результаты: p50/p95/p99 per-phase CPU, memory peak, draw calls.
- Baseline хранится в репозитории (JSON/CSV).

#### 5.2 Perf gate

- **Фаза A:** Warning в PR ("render sort +15% vs baseline").
- **Фаза B:** Blocking (после калибровки threshold'ов, не раньше чем через 2-3 месяца стабильных данных).

#### 5.3 Anti-flake

- Фиксированные seed во всех пресетах.
- Warmup iterations (первые N кадров отбрасываются из статистики).
- Hardware-pinned runner (без thermal throttling, без VM jitter).

**DoD:** Регрессии в spatial/render/query видны в PR. Ложные срабатывания < 5%.

---

### Фаза 6 — `predictedVisible` → actual drawn (P3, если потребуется)

**Когда:** Только если `predictedVisible` регулярно расходится с фактом и это мешает отладке.

**Текущий статус:** `predictedVisible` проверяет те же условия, что render pipeline, кроме `ecsView.contains()` и `entity != NullEntity`. Расхождение = баг (сломанный маппинг или потерянный компонент), а не штатная ситуация. Текущая точность достаточна.

**Если потребуется, два варианта:**

#### Вариант A: RenderSystem bitset (предпочтительный)

- `RenderSystem` хранит `std::vector<bool>` по spatial ID (размер = maxSpatialId).
- В gather-фазе: `mDrawnThisFrame[id] = true`.
- В `beginFrame()`: `clear()` (O(N/64) благодаря bitset packing).
- Метод `wasDrawnThisFrame(SpatialId32 id) const noexcept` → O(1).
- Cost: ~128KB для 1M entities, один memset per frame.

#### Вариант B: Факт через FrameStats (если нужен только count)

- `FrameStats::drawnEntityCount` уже есть как `spriteCount`.
- В overlay: `predicted=1 drawn=1` (сравнение prediction с `spriteCount > 0 && inQuery`).
- Грубее, но zero additional overhead.

**DoD:** В overlay `drawn=0/1` отражает факт прохождения через render pipeline.

---

### Фаза 7 — SimStress (future, P3, после появления 4X-кода)

**Когда:** Когда существует хотя бы прототип 4X simulation loop (тики, ходы, AI decisions).
**Цель:** Измерить бюджет тика под synthetic deterministic workloads.

**Не делать раньше, потому что:**
- Неизвестна модель симуляции (тиковая? пошаговая? гибрид?).
- Неизвестны write/read паттерны.
- Неизвестно соотношение CPU sim vs GPU render.

**Когда появится 4X-код:**
- Synthetic workloads на subset entities.
- Phase barriers (write → read → render).
- Tick budget measurement (target: <16ms для 60fps, <33ms для 30fps).

---

## Сводная таблица приоритетов

| Фаза | Приоритет | Когда | Зависимости | Effort | Статус |
|-------|-----------|-------|-------------|--------|--------|
| 0 — Observability cleanup | **P0** | — | — | — | ✅ DONE |
| 1 — Precision + presets | **P1** | — | Фаза 0 | — | ✅ DONE |
| 2 — RenderStress | **P1** | До/параллельно CMake | Фаза 1 | 1-2 недели | TODO |
| 3 — CMake migration | **Отдельный трек** | Скоро | Нет | — | TODO |
| 4 — QueryStress | **P2** | После CMake + 4X прототип | Фазы 1, 3 | 1-2 недели | TODO |
| 5 — CI/Bench | **P2** | После CMake + 2 stress modes | Фазы 2, 3 | 2 недели | TODO |
| 6 — predictedVisible → fact | **P3** | Если потребуется | Фаза 2 | 1-2 дня | BACKLOG |
| 7 — SimStress | **P3** | После 4X-кода | Фазы 4, 5 | 2+ недели | BACKLOG |

---

## Принципы (инварианты плана)

1. **YAGNI:** Не строить stress-режимы для несуществующего gameplay. Scaffold документом, реализовать когда появится потребность.
2. **Measure before optimize:** Каждый режим сначала даёт baseline, потом оптимизируем.
3. **Reproducibility:** Каждый прогон — пресет с фиксированным seed. Никаких "ручных ENV".
4. **Observability first:** Метрики должны быть однозначно читаемы без исходников. Двусмысленность дороже бага.
5. **Zero overhead in Release:** Все stress/debug метрики — `#if !defined(NDEBUG) || defined(RETRIBUTIO_PROFILE)`. В Release — zeroed structs, no-op methods.
6. **Validate-on-write, trust-on-read:** Конфиги валидируются на границе загрузки. Hot path не проверяет инварианты.