# Atrapacielos / Engine / Auctoritas — Architecture Roadmap

## Текущее состояние (достигнуто)

### CMake миграция — завершена полностью
- Ninja Multi-Config генератор: Debug / Profile / Release
- Все три конфигурации собираются чисто, инкрементальный билд работает
- vcpkg x64-windows-static-md, SFML 3.0.2 статическая линковка
- Кастомная конфигурация Profile полностью изолирована от Release по флагам
- Ogg/Vorbis/FLAC imported targets пропатчены под Ninja Multi-Config
- MSVC флаги точно соответствуют оригинальному .vcxproj
- PCH per-target (REUSE_FROM убран — несовместим с Ninja Multi-Config + MSVC)
- WIN32_EXECUTABLE + SFML::Main корректно подключены для Profile/Release
- retributio_engine_tests получил /wd4189 (C4189 на тест-переменных под NDEBUG — намеренный паттерн)

### Целевые таргеты (все собираются)
```
retributio_core                  — движок (STATIC)
retributio_engine_tests          — self-tests движка (STATIC, компилируется везде, линкуется Debug only)
retributio_atrapacielos          — игровой runtime Atrapacielos (STATIC)
retributio_atrapacielos_dev      — dev overlay, Debug + Profile (STATIC)
retributio_spatial_harness_lib   — spatial index harness (STATIC)
retributio_render_stress_lib     — render stress (INTERFACE, заглушка)
retributio_atrapacielos_game     — Atrapacielos EXE (все конфиги)
retributio_spatial_harness       — spatial harness EXE (все конфиги)
retributio_render_stress         — render stress EXE (все конфиги, заглушка)
```

### Entry points (все три — заглушки `int main() { return 0; }`)
Реальный код находится в библиотеках, не в main-файлах.

### Стресс-пресеты (есть, не интегрированы в CMake)
```
tools/presets/atrapacielos/active_small.env   — smoke: ~1K entities
tools/presets/atrapacielos/active_large.env   — soak: ~1M+ entities
```
Сейчас: ручной запуск через VS Debugging → Environment или shell.

---

## Известная архитектурная проблема (не сломано сейчас, сломается при реальном main)

```
retributio_atrapacielos_dev  ──uses──►  StressRuntimeStamp
                                        ↑
retributio_spatial_harness_lib  ──owns──┘  (stress_runtime_stamp.cpp)
           │
           └──►  retributio_atrapacielos (FrameOrchestrator)
```

`retributio_atrapacielos_game` (Profile) линкует `retributio_atrapacielos_dev`, но не линкует
`retributio_spatial_harness_lib` → когда main перестанет быть заглушкой: LNK2001.
Решение: Phase 1 ниже.

---

## Целевая архитектура (инвариант для всех фаз)

```
/                              <- repo root
├── engine/
│   ├── include/               <- публичные заголовки движка
│   ├── src/core/              <- retributio_core
│   ├── assets/                <- engine assets (config, fonts, placeholders)
│   └── CMakeLists.txt
│
├── games/
│   ├── atrapacielos/
│   │   ├── include/
│   │   ├── src/
│   │   ├── assets/
│   │   └── CMakeLists.txt
│   └── auctoritas/            <- создаётся в своё время, не раньше
│       └── CMakeLists.txt
│
├── tools/                     <- НИКОГДА не линкуется в release-бинари игр
│   ├── presets/
│   │   ├── atrapacielos/      <- .env файлы для Atrapacielos
│   │   └── auctoritas/        <- .env файлы для Auctoritas (placeholder)
│   ├── src/
│   └── CMakeLists.txt
│
├── tests/
│   ├── engine/
│   ├── atrapacielos/
│   └── CMakeLists.txt
│
├── third_party/
├── CMakeLists.txt
└── CMakePresets.json
```

**Правила, которые никогда не нарушаются:**
- `engine/` не содержит ни одного `#include` из `games/` или `tools/`
- `games/atrapacielos/` не знает о `games/auctoritas/` и наоборот
- `tools/` не линкуются ни в один release-бинарь игры
- Каждая игра собирается независимо: `cmake -DBUILD_ATRAPACIELOS=ON -DBUILD_AUCTORITAS=OFF`
- `tests/` — только Google Tests, гоняются в CI; `tools/` — только интерактивный профилинг

---

## Phase 1 — Fix архитектурной бомбы + stress_common
**Когда:** до написания реальных main-файлов. Сейчас, пока entry points заглушки.
**Что не трогаем:** физическую структуру директорий на диске (только CMake).

### Новый таргет: `retributio_stress_common`

Файлы, которые **переезжают** из `retributio_spatial_harness_lib`:
```
games/atrapacielos/src/dev/stress_runtime_stamp.cpp
games/atrapacielos/src/dev/stress_chunk_content_provider.cpp
```
(физически остаются на месте, только CMake ownership меняется)

Новый граф зависимостей:
```
retributio_stress_common
  <- stress_runtime_stamp.cpp
  <- stress_chunk_content_provider.cpp
  -> зависит от: retributio_core (ResourceManager, ChunkCoord, log)
  -> НЕ зависит от: retributio_atrapacielos, retributio_atrapacielos_dev

retributio_atrapacielos_dev
  -> добавляет зависимость: retributio_stress_common

retributio_spatial_harness_lib
  -> добавляет зависимость: retributio_stress_common
  -> убирает источники: stress_runtime_stamp.cpp, stress_chunk_content_provider.cpp

retributio_atrapacielos_game (Profile)
  -> добавляет зависимость: retributio_stress_common
     (нужна чтобы StressRuntimeStamp был доступен через retributio_atrapacielos_dev)
```

### Файлы которые нужны для Phase 1
```
games/atrapacielos/CMakeLists.txt   <- добавить retributio_stress_common, перераспределить источники
tools/CMakeLists.txt                <- обновить retributio_spatial_harness_lib
```
Всё остальное — без изменений.

### Пресеты: интеграция в CMake
Добавить custom target для удобства разработчика (не влияет на бинари):
```cmake
add_custom_target(stress_small
    COMMAND ${CMAKE_COMMAND}
        -DENV_FILE=${_presets_dir}/active_small.env
        -DEXE=$<TARGET_FILE:retributio_spatial_harness>
        -P "${_run_with_env}"
    DEPENDS retributio_spatial_harness
    COMMENT "[stress_small] retributio_spatial_harness + active_small.env"
    USES_TERMINAL
)
```
Запуск: `cmake --build --preset win-profile --target stress_small`.

**Acceptance gate Phase 1:**
- `cmake --build --preset win-profile` — чистый билд без предупреждений
- Граф зависимостей не содержит циклов
- `retributio_atrapacielos_game` Release не линкует ни одного байта из `tools/`

---

## Phase 2 — Google Tests + CI/CD foundation
**Когда:** параллельно с разработкой Atrapacielos, до релиза.

### Структура tests/
```
tests/
├── CMakeLists.txt
├── engine/
│   ├── CMakeLists.txt
│   └── src/
│       ├── string_pool_test.cpp
│       ├── resource_registry_test.cpp
│       └── resource_manager_test.cpp
└── atrapacielos/
    ├── CMakeLists.txt
    └── src/
        └── (будущие тесты)
```

### CMake
```cmake
option(BUILD_TESTS "Build Google Tests" ON)
if(BUILD_TESTS)
    find_package(GTest CONFIG REQUIRED)
    add_subdirectory(tests)
endif()
```

### Что меняется в существующих таргетах
- `retributio_engine_tests` (STATIC) — удаляется полностью
- Его источники переезжают в `tests/engine/` как Google Tests
- `retributio_atrapacielos_game` линкует на один таргет меньше (убрать `retributio_engine_tests` из link)
- `CMAKE_CONFIGURATION_TYPES` остаётся прежним (Debug/Profile/Release)
- Тесты гоняются через `ctest -C Debug --preset win-debug`

### GitHub Actions
```yaml
on: [push, pull_request]
jobs:
  build-and-test:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
        with: { submodules: recursive }
      - uses: lukka/run-vcpkg@v11
      - run: cmake --preset win-msvc-ninja-mc
      - run: cmake --build --preset win-debug
      - run: ctest --preset win-debug --output-on-failure
```

### Файлы которые нужны для Phase 2
```
tests/CMakeLists.txt
tests/engine/CMakeLists.txt
tests/engine/src/*.cpp            <- migrated + rewritten as GTest
.github/workflows/ci.yml
vcpkg.json                        <- добавить gtest dependency
games/atrapacielos/CMakeLists.txt <- убрать retributio_engine_tests
CMakeLists.txt                    <- добавить option(BUILD_TESTS) + add_subdirectory(tests)
```

**Acceptance gate Phase 2:**
- `ctest --preset win-debug` — все тесты зелёные
- GitHub Actions: зелёный значок на main
- `retributio_atrapacielos_game` Release не содержит ни одного тест-символа

---

## Phase 3 — Реальные entry points (Atrapacielos MVP)
**Когда:** основная разработка игры.

### main_atrapacielos.cpp
Заглушка `int main() { return 0; }` заменяется реальным кодом.
Зависит от: `retributio_atrapacielos`, `retributio_atrapacielos_dev` (Debug+Profile), `SFML::Main`.
Не знает о харнесах, тестах, stress-коде.

### main_spatial_harness.cpp
Подключает `SpatialIndexHarness`, читает env-переменные.
Запускается с пресетами из `tools/presets/atrapacielos/`.
Profile-билд выдаёт timing метрики в stdout (p50/p95/p99).

### main_render_stress.cpp
Создаёт SFML Window + GPU context.
Запускает `StressChunkContentProvider` сценарий.
Выдаёт frame timing метрики.
Не существует до тех пор пока не будет реального рендер-сценария.

### Файлы которые нужны для Phase 3
```
games/atrapacielos/src/main_atrapacielos.cpp   <- реальный код
tools/src/main_spatial_harness.cpp             <- реальный код
tools/src/main_render_stress.cpp               <- реальный код (позже)
```

**Acceptance gate Phase 3:**
- `retributio_atrapacielos_game` Debug запускается, показывает окно
- `retributio_spatial_harness` Profile с `active_large.env` выдаёт метрики, не крашится
- `retributio_atrapacielos_game` Release не содержит стресс-кода (проверка dumpbin на отсутствие символов)

---

## Phase 4 — Физическое разделение engine/ и games/ в CMake
**Когда:** после релиза Atrapacielos, перед началом Auctoritas.

### Что меняется
Root `CMakeLists.txt` перестаёт знать о конкретных играх:
```cmake
option(BUILD_ATRAPACIELOS "Build Atrapacielos game" ON)
option(BUILD_AUCTORITAS   "Build Auctoritas game"   OFF)
option(BUILD_TOOLS        "Build dev tools"          ON)
option(BUILD_TESTS        "Build tests"              ON)

add_subdirectory(engine)

if(BUILD_ATRAPACIELOS) add_subdirectory(games/atrapacielos) endif()
if(BUILD_AUCTORITAS)   add_subdirectory(games/auctoritas)   endif()
if(BUILD_TOOLS)        add_subdirectory(tools)              endif()
if(BUILD_TESTS)        add_subdirectory(tests)              endif()
```

`engine/CMakeLists.txt` — standalone, компилируется без игр.

### Критическое архитектурное решение для engine/
Сейчас `retributio_core` напрямую использует SFML типы (`sf::IntRect`, `sf::Vector2f` etc).
Это нужно будет решить одним из двух способов:
- **A (прагматично):** движок зависит от SFML как от math/types library, обе игры используют один рендер-бэкенд. Продаётся как "engine built on SFML".
- **B (максимальная независимость):** вводим `engine::Vec2f`, `engine::IntRect` — собственные типы, SFML остаётся деталью реализации. Больше работы.

Это решение принимается **до начала Auctoritas**, не раньше.

### Файлы которые нужны для Phase 4
```
CMakeLists.txt                       <- root переписывается
engine/CMakeLists.txt                <- новый, содержит retributio_core
games/atrapacielos/CMakeLists.txt    <- новый, содержит atrapacielos таргеты
tools/CMakeLists.txt                 <- новый, содержит stress_common + харнесы
CMakePresets.json                    <- обновить пути если изменятся
```
Физическое перемещение `.cpp/.h` файлов — отдельный коммит после CMake.

**Acceptance gate Phase 4:**
```
cmake -DBUILD_ATRAPACIELOS=OFF -DBUILD_AUCTORITAS=OFF
      -DBUILD_TOOLS=OFF -DBUILD_TESTS=OFF --preset win-msvc-ninja-mc
cmake --build --preset win-release
```
Должен собраться чистый движок без единого игрового символа.

---

## Phase 5 — Auctoritas (начало)
**Когда:** после Phase 4 Acceptance gate.

- `games/auctoritas/CMakeLists.txt` создаётся с нуля
- Auctoritas не содержит ни одного `#include` из `games/atrapacielos/`
- Auctoritas использует `engine` как зависимость, не как subdirectory с игровым кодом
- Стресс-инфраструктура (`retributio_stress_common`) переосмысляется для масштаба 1-2M entities:
  новые пресеты в `tools/presets/auctoritas/`, возможно отдельный харнес `tools/src/auctoritas_harness/`

---

## Текущие пресеты — правильное место

```
tools/
└── presets/
    └── atrapacielos/
        ├── active_small.env   <- уже есть
        ├── active_large.env   <- уже есть
        └── README.md          <- уже есть
```

Пресеты остаются как есть — это правильный 12-factor подход.
В Phase 1 добавляем CMake custom targets для удобства:
```cmake
# запуск через: cmake --build --preset win-profile --target stress_small
add_custom_target(stress_small ...)
add_custom_target(stress_large ...)
```

---

## Сводная таблица фаз

| Phase | Что делаем | Файлы меняются | Файлы на диске двигаются |
|-------|-----------|---------------|--------------------------|
| 1 | retributio_stress_common, fix deps | games/atrapacielos/CMakeLists.txt, tools/CMakeLists.txt | НЕТ |
| 2 | Google Tests + CI/CD | tests/*, .github/*, vcpkg.json, CMakeLists | НЕТ |
| 3 | Реальные main-файлы | 3 entry point .cpp | НЕТ |
| 4 | Физическое разделение engine/games/tools | root CMakeLists + новые CMakeLists | ДА |
| 5 | Auctoritas начало | games/auctoritas/* | ДА |

Phases 1-3 выполняются без перемещения файлов на диске — это критично для скорости Atrapacielos MVP.
Phase 4 — полный рефакторинг структуры директорий, один большой коммит.
