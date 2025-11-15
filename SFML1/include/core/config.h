#pragma once
#ifdef _MSC_VER
#pragma warning(disable : 4514 4623 4625 4626 4668 4820 5027 5039 5246 5267 4061 4365 4100)
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <string>

#include <SFML/System/Clock.hpp>
#include <SFML/System/Time.hpp>
#include <SFML/Window/Keyboard.hpp>

// ============================================================================
// config.h — централизованная конфигурация проекта
// ============================================================================
// Здесь задаются глобальные константы и флаги, влияющие на поведение игры.
// Этот файл не содержит логики — только настройки и опции.
//
// В будущем можно будет заменить на систему конфигов из JSON или INI.
// ============================================================================

namespace config {

    // ---------------------------------------------------------------------------------
    // Структура конфигурации окна
    // ---------------------------------------------------------------------------------

    inline constexpr unsigned int WINDOW_WIDTH = 1024;
    inline constexpr unsigned int WINDOW_HEIGHT = 768;
    inline constexpr const char* WINDOW_TITLE = "SkyGuard (name subject to change)";

    // ---------------------------------------------------------------------------------
    // Игровые параметры по умолчанию (могут быть переопределены в JSON)
    // ---------------------------------------------------------------------------------
    //
    inline const std::string PLAYER_TEXTURE =
        "assets/images/player.png"; // путь к дефолтной текстуре игрока
    inline constexpr float PLAYER_SPEED =
        100.f; // скорость игрока в пикселях в секунду по умолчанию
    inline constexpr float PLAYER_ACCELERATION = 800.f; // ускорение игрока
    inline constexpr float PLAYER_FRICTION = 6.f; // коэффициент замедления при отпускании клавиш

    // Дефолтные клавиши управления (могут быть переопределены в JSON)
    inline constexpr sf::Keyboard::Key DEFAULT_KEY_UP = sf::Keyboard::Key::W;
    inline constexpr sf::Keyboard::Key DEFAULT_KEY_DOWN = sf::Keyboard::Key::S;
    inline constexpr sf::Keyboard::Key DEFAULT_KEY_LEFT = sf::Keyboard::Key::A;
    inline constexpr sf::Keyboard::Key DEFAULT_KEY_RIGHT = sf::Keyboard::Key::D;

    // Дефолтные строковые режимы (могут быть переопределены в JSON)
    inline constexpr const char* DEFAULT_SCALING = "None";            // None | Uniform
    inline constexpr const char* DEFAULT_LOCK_BEHAVIOR = "WorldLock"; // WorldLock | ScreenLock
    inline constexpr const char* DEFAULT_ANCHOR = "BottomCenter";

    // Фиксированный шаг логики (1/60)
    inline const sf::Time FIXED_TIME_STEP = sf::seconds(1.f / 60.f);

    // Настройки рендеринга / синхронизации
    // Если true -> используем вертикальную синхронизацию (VSync), а ограничение FPS игнорируется.
    inline constexpr bool ENABLE_VSYNC = true;

    // Ограничение FPS через setFramerateLimit (0 — отключено).
    // Игнорируется, если ENABLE_VSYNC == true.
    inline constexpr unsigned int FRAME_LIMIT = 0;

// ---------------------------------------------------------------------------------
// Переключатели (toggles) для debug_overlay_system
// SHOW_FPS_OVERLAY:
//   - Значение по умолчанию для рантайм-состояния класса DebugOverlaySystem
//   - Может быть перезаписано также в рантайм через HOTKEY_TOGGLE_OVERLAY
// HOTKEY_TOGGLE_OVERLAY:
//   - Быстрый переключатель для debug_overlay; позже вынести в слой input bindings
// ---------------------------------------------------------------------------------
// Показывать ли оверлей по умолчанию: debug - true, release - false
#ifdef _DEBUG
    inline constexpr bool SHOW_FPS_OVERLAY = true;
#else
    inline constexpr bool SHOW_FPS_OVERLAY = false;
#endif
    // Хоткей для тоггла (оставляем и в релизе: неактивно, если оверлей не добавлен/выключен)
    inline constexpr sf::Keyboard::Key HOTKEY_TOGGLE_OVERLAY = sf::Keyboard::Key::F3;

    // ---------------------------------------------------------------------------------
    // Отладка
    // ---------------------------------------------------------------------------------

    // Показывать ли сообщение при завершении программы (для отладки)
    inline constexpr bool DEBUG_HOLD_ON_EXIT = true;

} // namespace config
