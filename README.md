# SkyGuard (name subject to change)

A 2D aerial combat indie game built with **C++17** and **SFML 3.0.2**.

## Features
- Smooth player movement with configurable speed/scale via JSON
- ResourceManager for textures and fonts
- Config-driven architecture (C++ + JSON)
- Modular core/entity system

## Project Structure
```text
SFML1/
├─ assets/                # Игровые ресурсы
│  ├─ config/             # Конфиги JSON
│  ├─ fonts/              # Шрифты
│  ├─ images/             # Текстуры и спрайты
│  └─ sounds/             # Звуки и музыка
├─ data/                  # Технические данные движка
│  └─ definitions/        # JSON определения ресурсов (например, resources.json)
├─ include/               # Заголовочные файлы
│  ├─ core/               # Ядро движка (Game, ResourceManager, TimeSystem, TextOutput)
│  ├─ entities/           # Сущности (Player, NPC и т.д.)
│  ├─ graphics/           # Вспомогательная графика и утилиты
│  ├─ third_party/        # Сторонние библиотеки
│  └─ utils/              # Вспомогательные функции и утилиты
├─ src/                   # Исходники проекта
├─ CREDITS.md             # Список участников проекта
└─ LICENSE.MIT            # Лицензия MIT

## Build
1. Install [SFML 3.0.2](https://www.sfml-dev.org/download/sfml/3.0.2/)
2. Open the project in Visual Studio
3. Build and run

## License
MIT License. See [LICENSE.MIT](./third_party/LICENSE.MIT).