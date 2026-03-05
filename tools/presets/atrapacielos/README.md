# Atrapacielos Stress Presets

## Файлы

| Пресет | Назначение | Примерный масштаб |
|--------|-----------|-------------------|
| `active_small.env` | Smoke: корректность spatial index, метрики читаемы | ~198 entities (8 × 4×4 chunks) |
| `active_large.env` | Soak: spatial index capacity, streaming, memory ceiling | ~25K entities (64 × 128×128 chunks) |

## ENV-ключи

### Стресс-контент игры (`retributio_atrapacielos_game` — StressChunkContentProvider)

| Ключ | Описание |
|------|----------|
| `ATRAPACIELOS_STRESS_ENABLED` | Включить stress mode (1/0) |
| `ATRAPACIELOS_STRESS_SEED` | Seed детерминированного генератора |
| `ATRAPACIELOS_STRESS_ENTITIES_PER_CHUNK` | Количество сущностей на чанк |
| `ATRAPACIELOS_STRESS_TEXTURE_COUNT` | Количество текстур для ротации |
| `ATRAPACIELOS_STRESS_Z_LAYERS` | Количество z-уровней (sort pressure) |
| `ATRAPACIELOS_STRESS_WINDOW_WIDTH` | Ширина окна стриминга в чанках |
| `ATRAPACIELOS_STRESS_WINDOW_HEIGHT` | Высота окна стриминга в чанках |
| `ATRAPACIELOS_STRESS_TEXTURE_IDS` | CSV индексов текстур (опционально, вместо COUNT) |

### Spatial harness (`retributio_spatial_harness` — изолированный бенчмарк)

| Ключ | Описание |
|------|----------|
| `ATRAPACIELOS_SPATIAL_HARNESS` | Запустить харнес (1/0). Без этого ключа EXE тихо выходит. |
| `ATRAPACIELOS_SPATIAL_HARNESS_ENTITIES` | Число синтетических entities (дефолт: min(25000, maxEntityId)) |
| `ATRAPACIELOS_SPATIAL_HARNESS_QUERIES` | Число query-итераций (дефолт: 128, макс: 10000) |
| `ATRAPACIELOS_SPATIAL_HARNESS_UPDATES` | Число update-проходов (дефолт: 8, макс: 1000) |

`ATRAPACIELOS_SPATIAL_HARNESS=1` уже зашит в оба `.env` файла — при запуске через CMake targets
он выставляется автоматически.

## Запуск

### CMake (основной способ)

```cmd
cmake --build --preset win-profile --target stress_small
cmake --build --preset win-profile --target stress_large
```

Собирает `retributio_spatial_harness` если нужно, выставляет все ENV из `.env` файла,
запускает EXE. Результаты — в `out/build/win-msvc-ninja-mc/Retributio/Profile/logs/`.

Увеличить масштаб теста для stress_large:
```
# Добавить в active_large.env:
ATRAPACIELOS_SPATIAL_HARNESS_ENTITIES=500000
```

### MSVC — стресс-контент в игре (retributio_atrapacielos_game)

Project → Properties → Debugging → Environment. Вставить (без строк-комментариев):

```
ATRAPACIELOS_STRESS_ENABLED=1
ATRAPACIELOS_STRESS_SEED=42
ATRAPACIELOS_STRESS_ENTITIES_PER_CHUNK=64
ATRAPACIELOS_STRESS_TEXTURE_COUNT=3
ATRAPACIELOS_STRESS_Z_LAYERS=5
ATRAPACIELOS_STRESS_WINDOW_WIDTH=128
ATRAPACIELOS_STRESS_WINDOW_HEIGHT=128
```

Запускает игру с 1M+ реальных ECS-сущностей, overlay показывает метрики в реальном времени.
Это тестирует стриминг, рендер, ECS — не только spatial index.

### MSVC — spatial harness через MSVC

Для `retributio_spatial_harness` в Properties → Debugging → Environment:

```
ATRAPACIELOS_SPATIAL_HARNESS=1
ATRAPACIELOS_STRESS_ENABLED=1
ATRAPACIELOS_STRESS_ENTITIES_PER_CHUNK=64
ATRAPACIELOS_STRESS_WINDOW_WIDTH=128
ATRAPACIELOS_STRESS_WINDOW_HEIGHT=128
```

Запускает изолированный бенчмарк spatial index без игрового цикла и рендера.

## Что тестирует каждый инструмент

| Инструмент | Что тестирует | GUI |
|------------|---------------|-----|
| `retributio_atrapacielos_game` + `ATRAPACIELOS_STRESS_*` | ECS + стриминг + рендер + spatial на реальных данных | Да |
| `retributio_spatial_harness` + `ATRAPACIELOS_SPATIAL_HARNESS=1` | Только SpatialIndexV2 в изоляции: query/update тайминги, V1 vs V2 корректность | Нет |
| `retributio_render_stress` (будущий) | GPU рендер + frame timing на стресс-сцене | Да |

## KPI

- **active_small:** харнес завершается без крашей, все сценарии passed, метрики читаемы.
- **active_large:** V2 query быстрее V1, overflow нулевой, MarksMaintenanceBeforeUpdate passed.