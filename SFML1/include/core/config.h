#pragma once

#include <string>
#include <SFML/System/Clock.hpp>
#include <SFML/System/Time.hpp>

// ============================================================================
// config.h — централизованная конфигурация проекта
// ============================================================================
// Здесь задаются глобальные константы и флаги, влияющие на поведение игры.
// Этот файл не содержит логики — только настройки и опции.
//
// В будущем можно будет заменить на систему конфигов из JSON или INI.
// ============================================================================

namespace config {

    // -----------------------
    // Основные параметры окна приложения
    // -----------------------
    
    inline constexpr unsigned int WINDOW_WIDTH = 1024;
    inline constexpr unsigned int WINDOW_HEIGHT = 768;
    inline constexpr const char* WINDOW_TITLE = "SkyGuard (name subject to change)";

    // -----------------------
    // Игровые параметры
    // -----------------------
    //      
    inline const std::string PLAYER_TEXTURE = "assets/images/player.png"; // путь к дефолтной текстуре игрока
    
    inline constexpr float PLAYER_SPEED = 100.f; // скорость игрока в пикселях в секунду по умолчанию,
                                                 // если не загружена из JSON

    // Фиксированный шаг логики (1/60)
    inline const sf::Time FIXED_TIME_STEP = sf::seconds(1.f / 60.f);

    // Настройки рендеринга / синхронизации
    // Если true -> используем вертикальную синхронизацию (VSync), а ограничение FPS игнорируется.
    inline constexpr bool ENABLE_VSYNC = true;

    // Ограничение FPS через setFramerateLimit (0 — отключено).
    // Игнорируется, если ENABLE_VSYNC == true.
    inline constexpr unsigned int FRAME_LIMIT = 0;
     
    // -----------------------
    // Отладка
    // -----------------------
 
    // Показывать ли сообщение при завершении программы (для отладки)
    inline constexpr bool DEBUG_HOLD_ON_EXIT = true;

} // namespace config

