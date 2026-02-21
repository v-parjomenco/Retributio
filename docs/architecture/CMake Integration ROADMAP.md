# SkyGuard / Engine / Titan — Architecture Roadmap

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
- sfml1_core_tests получил /wd4189 (C4189 на тест-переменных под NDEBUG — намеренный паттерн)

### Целевые таргеты (все собираются)
```
sfml1_core               — движок (STATIC)
sfml1_core_tests         — self-tests движка (STATIC, компилируется везде, линкуется Debug only)
sfml1_skyguard           — игровой runtime SkyGuard (STATIC)
sfml1_skyguard_dev       — dev overlay, Debug + Profile (STATIC)
sfml1_spatial_harness_lib — spatial index harness (STATIC)
sfml1_render_stress_lib  — render stress (INTERFACE, заглушка)
sfml1_game               — SkyGuard EXE (все конфиги)
sfml1_spatial_harness    — spatial harness EXE (все конфиги)
sfml1_render_stress      — render stress EXE (все конфиги, заглушка)
```

### Entry points (все три — заглушки `int main() { return 0; }`)
Реальный код находится в библиотеках, не в main-файлах.

### Стресс-пресеты (есть, не интегрированы в CMake)
```
tools/presets/active_small.env   — smoke: ~1K entities
tools/presets/active_large.env   — soak: ~1M+ entities
```
Сейчас: ручной запуск через VS Debugging → Environment или shell.

---

## Известная архитектурная проблема (не сломано сейчас, сломается при реальном main)

```
sfml1_skyguard_dev  ──uses──►  StressRuntimeStamp
                                       ↑
sfml1_spatial_harness_lib  ──owns──────┘  (stress_runtime_stamp.cpp)
           │
           └──►  sfml1_skyguard (FrameOrchestrator)
```

`sfml1_game` (Profile) линкует `sfml1_skyguard_dev`, но не линкует
`sfml1_spatial_harness_lib` → когда main перестанет быть заглушкой: LNK2001.
Решение: Phase 1 ниже.

---

## Целевая архитектура (инвариант для всех фаз)

```
SFML1/
├── engine/
│   ├── include/               ← публичные заголовки движка
│   ├── src/core/              ← текущий sfml1_core (перенос без изменений)
│   ├── third_party/           ← xxhash и прочее (текущее положение)
│   └── CMakeLists.txt
│
├── games/
│   ├── skyguard/
│   │   ├── include/
│   │   ├── src/
│   │   └── CMakeLists.txt
│   └── titan/                 ← создаётся в своё время, не раньше
│       └── CMakeLists.txt
│
├── tools/                     ← НИКОГДА не линкуется в release-бинари игр
│   ├── presets/               ← текущие .env файлы (уже есть)
│   ├── stress_common/         ← Phase 1: новый таргет
│   │   ├── include/
│   │   └── src/
│   ├── spatial_harness/       ← Phase 1: перенос из sfml1_spatial_harness_lib
│   │   ├── include/
│   │   └── src/
│   └── render_stress/         ← Phase 3: когда будет реализован
│       ├── include/
│       └── src/
│
├── tests/                     ← Phase 2: Google Tests + CI/CD
│   ├── engine/
│   └── skyguard/
│
├── CMakeLists.txt             ← root
└── CMakePresets.json
```

**Правила, которые никогда не нарушаются:**
- `engine/` не содержит ни одного `#include` из `games/` или `tools/`
- `games/skyguard/` не знает о `games/titan/` и наоборот
- `tools/` не линкуются ни в один release-бинарь игры
- Каждая игра собирается независимо: `cmake -DBUILD_SKYGUARD=ON -DBUILD_TITAN=OFF`
- `tests/` — только Google Tests, гоняются в CI; `tools/` — только интерактивный профилинг

---

## Phase 1 — Fix архитектурной бомбы + stress_common
**Когда:** до написания реальных main-файлов. Сейчас, пока entry points заглушки.
**Что не трогаем:** физическую структуру директорий на диске (только CMake).

### Новый таргет: `sfml1_stress_common`

Файлы, которые **переезжают** из `sfml1_spatial_harness_lib`:
```
SFML1/src/game/skyguard/dev/stress_runtime_stamp.cpp
SFML1/src/game/skyguard/dev/stress_chunk_content_provider.cpp
```
(физически остаются на месте, только CMake ownership меняется)

Новый граф зависимостей:
```
sfml1_stress_common
  ← stress_runtime_stamp.cpp
  ← stress_chunk_content_provider.cpp
  → зависит от: sfml1_core (ResourceManager, ChunkCoord, log)
  → НЕ зависит от: sfml1_skyguard, sfml1_skyguard_dev

sfml1_skyguard_dev
  → добавляет зависимость: sfml1_stress_common

sfml1_spatial_harness_lib
  → добавляет зависимость: sfml1_stress_common
  → убирает источники: stress_runtime_stamp.cpp, stress_chunk_content_provider.cpp

sfml1_game (Profile)
  → добавляет зависимость: sfml1_stress_common
     (нужна чтобы StressRuntimeStamp был доступен через sfml1_skyguard_dev)
```

### Файлы которые нужны для Phase 1
```
SFML1/CMakeLists.txt        ← добавить sfml1_stress_common, перераспределить источники
```
Всё остальное — без изменений.

### Пресеты: интеграция в CMake
Добавить custom target для удобства разработчика (не влияет на бинари):
```cmake
# В SFML1/CMakeLists.txt или tools/CMakeLists.txt:
add_custom_target(run_stress_small
    COMMAND ${CMAKE_COMMAND} -E env --modify-file=tools/presets/active_small.env
            $<TARGET_FILE:sfml1_spatial_harness>
    COMMENT "Running spatial harness with small stress preset"
)
```
Либо документировать как `cmake --build --preset win-profile --target run_stress_small`.
Альтернатива: оставить .env как есть — это тоже профессиональный подход (12-factor app),
просто добавить в README секцию "Running with preset" с конкретными командами.

**Acceptance gate Phase 1:**
- `cmake --build --preset win-profile` — чистый билд без предупреждений
- Граф зависимостей не содержит циклов
- `sfml1_game` Release не линкует ни одного байта из `tools/`

---

## Phase 2 — Google Tests + CI/CD foundation
**Когда:** параллельно с разработкой SkyGuard, до релиза.

### Структура tests/
```
tests/
├── CMakeLists.txt
├── engine/
│   ├── CMakeLists.txt
│   └── src/
│       ├── string_pool_test.cpp        ← migrate из sfml1_core_tests
│       ├── resource_registry_test.cpp  ← migrate из sfml1_core_tests
│       └── resource_manager_test.cpp   ← migrate из sfml1_core_tests
└── skyguard/
    ├── CMakeLists.txt
    └── src/
        └── (будущие тесты)
```

### CMake
```cmake
# root CMakeLists.txt
option(BUILD_TESTS "Build Google Tests" ON)
if(BUILD_TESTS)
    find_package(GTest CONFIG REQUIRED)  # через vcpkg
    add_subdirectory(tests)
endif()
```

### Что меняется в существующих таргетах
- `sfml1_core_tests` (STATIC) — удаляется полностью
- Его источники переезжают в `tests/engine/` как Google Tests
- `sfml1_game` линкует на один таргет меньше (убрать `sfml1_core_tests` из link)
- `CMAKE_CONFIGURATION_TYPES` остаётся прежним (Debug/Profile/Release)
- Тесты гоняются через `ctest -C Debug --preset win-debug`

### GitHub Actions
```yaml
# .github/workflows/ci.yml
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
tests/engine/src/*.cpp           ← migrated + rewritten as GTest
.github/workflows/ci.yml
vcpkg.json                       ← добавить gtest dependency
SFML1/CMakeLists.txt             ← убрать sfml1_core_tests
root CMakeLists.txt              ← добавить option(BUILD_TESTS) + add_subdirectory(tests)
```

**Acceptance gate Phase 2:**
- `ctest --preset win-debug` — все тесты зелёные
- GitHub Actions: зелёный значок на main
- `sfml1_game` Release не содержит ни одного тест-символа

---

## Phase 3 — Реальные entry points (SkyGuard MVP)
**Когда:** основная разработка игры.

### main_skyguard.cpp
Заглушка `int main() { return 0; }` заменяется реальным кодом.
Зависит от: `sfml1_skyguard`, `sfml1_skyguard_dev` (Debug+Profile), `SFML::Main`.
Не знает о харнесах, тестах, stress-коде.

### main_spatial_harness.cpp
Подключает `SpatialIndexHarness`, читает `SKYGUARD_SPATIAL_HARNESS` env.
Запускается с пресетами из `tools/presets/`.
Profile-билд выдаёт timing метрики в stdout (p50/p95/p99).

### main_render_stress.cpp
Создаёт SFML Window + GPU context.
Запускает `StressChunkContentProvider` сценарий.
Выдаёт frame timing метрики.
Не существует до тех пор пока не будет реального рендер-сценария.

### Файлы которые нужны для Phase 3
```
SFML1/src/game/skyguard/entry/main_skyguard.cpp    ← реальный код
SFML1/src/game/skyguard/entry/main_spatial_harness.cpp  ← реальный код
SFML1/src/game/skyguard/entry/main_render_stress.cpp    ← реальный код (позже)
```

**Acceptance gate Phase 3:**
- `sfml1_game` Debug запускается, показывает окно
- `sfml1_spatial_harness` Profile с `active_large.env` выдаёт метрики, не крашится
- `sfml1_game` Release не содержит стресс-кода (проверка nm/dumpbin на отсутствие символов)

---

## Phase 4 — Физическое разделение engine/ и games/ в CMake
**Когда:** после релиза SkyGuard, перед началом Titan.

### Что меняется
Root `CMakeLists.txt` перестаёт знать о конкретных играх:
```cmake
option(BUILD_SKYGUARD "Build SkyGuard game" ON)
option(BUILD_TITAN    "Build Titan game"    OFF)
option(BUILD_TOOLS    "Build dev tools"     ON)
option(BUILD_TESTS    "Build tests"         ON)

add_subdirectory(engine)

if(BUILD_SKYGUARD) add_subdirectory(games/skyguard) endif()
if(BUILD_TITAN)    add_subdirectory(games/titan)    endif()
if(BUILD_TOOLS)    add_subdirectory(tools)          endif()
if(BUILD_TESTS)    add_subdirectory(tests)          endif()
```

`engine/CMakeLists.txt` — standalone, компилируется без игр:
```cmake
cmake_minimum_required(VERSION 3.25)
project(SFMLEngine VERSION 1.0.0)
# Никаких find_package(SFML) здесь — движок не зависит от конкретного рендер-бэкенда напрямую
# SFML как implementation detail игр, не движка (или через абстракцию)
```

### Критическое архитектурное решение для engine/
Сейчас `sfml1_core` напрямую использует SFML типы (`sf::IntRect`, `sf::Vector2f` etc).
Это нужно будет решить одним из двух способов:
- **A (прагматично для Titan):** движок зависит от SFML как от math/types library, обе игры используют один рендер-бэкенд. Titan тоже на SFML. Продаётся как "engine built on SFML".
- **B (максимальная независимость):** вводим `engine::Vec2f`, `engine::IntRect` — собственные типы, SFML остаётся деталью реализации. Titan может использовать другой рендер-бэкенд. Больше работы.

Это решение принимается **до начала Titan**, не раньше.

### Файлы которые нужны для Phase 4
```
CMakeLists.txt                    ← root переписывается
engine/CMakeLists.txt             ← новый, содержит бывший sfml1_core
games/skyguard/CMakeLists.txt     ← новый, содержит бывшие skyguard таргеты
tools/CMakeLists.txt              ← новый, содержит stress_common + харнесы
CMakePresets.json                 ← обновить пути если изменятся
```
Физическое перемещение `.cpp/.h` файлов — отдельный коммит после CMake.

**Acceptance gate Phase 4:**
```
cmake -DBUILD_SKYGUARD=OFF -DBUILD_TITAN=OFF -DBUILD_TOOLS=OFF -DBUILD_TESTS=OFF --preset win-msvc-ninja-mc
cmake --build --preset win-release
```
Должен собраться чистый движок без единого игрового символа.

---

## Phase 5 — Titan (начало)
**Когда:** после Phase 4 Acceptance gate.

- `games/titan/CMakeLists.txt` создаётся с нуля
- Titan не содержит ни одного `#include` из `games/skyguard/`
- Titan использует `engine` как зависимость, не как subdirectory с игровым кодом
- Стресс-инфраструктура (`tools/stress_common`) переосмысляется для масштаба 1-2M entities:
  новые пресеты `titan_stress_*.env`, возможно отдельный харнес `tools/titan_harness/`
- Пресеты для SkyGuard и Titan хранятся в разных директориях: `tools/presets/skyguard/`, `tools/presets/titan/`

---

## Текущие пресеты — правильное место

```
tools/
└── presets/
    └── skyguard/
        ├── active_small.env   ← уже есть
        ├── active_large.env   ← уже есть
        └── README.md          ← уже есть
```

Пресеты остаются как есть — это правильный 12-factor подход.
В Phase 1 добавляем CMake custom targets для удобства:
```cmake
# запуск через: cmake --build --preset win-profile --target stress_small
add_custom_target(stress_small ...)
add_custom_target(stress_large ...)
```
Либо просто документируем PowerShell/cmd workflow из README — оба варианта профессиональны.

---

## Сводная таблица фаз

| Phase | Что делаем | Файлы меняются | Файлы на диске двигаются |
|-------|-----------|---------------|--------------------------|
| 1 | sfml1_stress_common, fix deps | SFML1/CMakeLists.txt | НЕТ |
| 2 | Google Tests + CI/CD | tests/*, .github/*, vcpkg.json, root+SFML1 CMakeLists | НЕТ |
| 3 | Реальные main-файлы | 3 entry point .cpp | НЕТ |
| 4 | Физическое разделение engine/games/tools | root CMakeLists + новые CMakeLists | ДА |
| 5 | Titan начало | games/titan/* | ДА |

Phases 1-3 выполняются без перемещения файлов на диске — это критично для скорости SkyGuard MVP.
Phase 4 — полный рефакторинг структуры директорий, один большой коммит.
