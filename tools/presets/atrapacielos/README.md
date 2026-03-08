# Atrapacielos Stress Presets

## Структура

- `stress_spatial/` — изолированный spatial harness (`retributio_stress_spatial`).
- `stress_render/` — интерактивный render stress (`retributio_stress_render`).

## ENV-ключи

### Spatial stress namespace (`ATRAPACIELOS_STRESS_SPATIAL_*`)

| Ключ | Описание |
|------|----------|
| `ATRAPACIELOS_STRESS_SPATIAL_ENABLED` | Включить stress content provider (1/0) |
| `ATRAPACIELOS_STRESS_SPATIAL_SEED` | Seed детерминированного генератора |
| `ATRAPACIELOS_STRESS_SPATIAL_ENTITIES_PER_CHUNK` | Количество сущностей на чанк |
| `ATRAPACIELOS_STRESS_SPATIAL_TEXTURE_COUNT` | Количество текстур для ротации |
| `ATRAPACIELOS_STRESS_SPATIAL_TEXTURE_IDS` | CSV индексов текстур (опционально, вместо COUNT) |
| `ATRAPACIELOS_STRESS_SPATIAL_Z_LAYERS` | Количество z-уровней (sort pressure) |
| `ATRAPACIELOS_STRESS_SPATIAL_WINDOW_WIDTH` | Ширина окна стриминга в чанках |
| `ATRAPACIELOS_STRESS_SPATIAL_WINDOW_HEIGHT` | Высота окна стриминга в чанках |

### Render stress namespace (`ATRAPACIELOS_STRESS_RENDER_*`)

`retributio_stress_render` читает этот namespace и маппит его в `ATRAPACIELOS_STRESS_SPATIAL_*`
перед запуском `game::atrapacielos::Game`.

| Ключ | Описание |
|------|----------|
| `ATRAPACIELOS_STRESS_RENDER_ENABLED` | Включить render stress run (1/0) |
| `ATRAPACIELOS_STRESS_RENDER_SEED` | Seed |
| `ATRAPACIELOS_STRESS_RENDER_ENTITIES_PER_CHUNK` | Сущностей на чанк |
| `ATRAPACIELOS_STRESS_RENDER_TEXTURE_COUNT` | Количество текстур |
| `ATRAPACIELOS_STRESS_RENDER_TEXTURE_IDS` | CSV индексов текстур |
| `ATRAPACIELOS_STRESS_RENDER_Z_LAYERS` | Z-слои |
| `ATRAPACIELOS_STRESS_RENDER_WINDOW_WIDTH` | Окно стриминга (чанки), X |
| `ATRAPACIELOS_STRESS_RENDER_WINDOW_HEIGHT` | Окно стриминга (чанки), Y |
| `ATRAPACIELOS_STRESS_RENDER_OVERLAY_BACKPLATE` | Включить чёрный полупрозрачный backplate overlay (1/0) |

### Stress spatial harness namespace

| Ключ | Описание |
|------|----------|
| `ATRAPACIELOS_STRESS_SPATIAL_HARNESS` | Запустить харнес (1/0) |
| `ATRAPACIELOS_STRESS_SPATIAL_HARNESS_ENTITIES` | Число synthetic entities |
| `ATRAPACIELOS_STRESS_SPATIAL_HARNESS_QUERIES` | Число query-итераций |
| `ATRAPACIELOS_STRESS_SPATIAL_HARNESS_UPDATES` | Число update-проходов |

## CMake targets

```cmd
cmake --build --preset win-profile --target spatial_small
cmake --build --preset win-profile --target spatial_large
cmake --build --preset win-profile --target render_small
cmake --build --preset win-profile --target render_large
```

- `spatial_*` запускают `retributio_stress_spatial`.
- `render_*` запускают `retributio_stress_render` (интерактивный режим, Profile-only).

## Presets

- `stress_spatial/active_small.env` — smoke spatial harness.
- `stress_spatial/active_large.env` — soak spatial harness.
- `stress_render/render_small.env` — ~1.0M active entities target.
- `stress_render/render_large.env` — ~2.0M active entities target.

## Матрица инструментов

| Инструмент | Что тестирует | GUI |
|------------|---------------|-----|
| `retributio_atrapacielos_game` + `ATRAPACIELOS_STRESS_SPATIAL_*` | ECS + стриминг + рендер + spatial на реальных данных | Да |
| `retributio_stress_spatial` + `ATRAPACIELOS_STRESS_SPATIAL_HARNESS=1` | Только SpatialIndexV2 в изоляции | Нет |
| `retributio_stress_render` + `ATRAPACIELOS_STRESS_RENDER_*` | Полный игровой loop + render stress (interactive Profile) | Да |