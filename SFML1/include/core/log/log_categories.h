// ================================================================================================
// File: core/log/log_categories.h
// Purpose: Predefined logging categories for the engine and games.
// Used by: core/log/log_macros.h, gameplay code.
// ================================================================================================
#pragma once

#include <string_view>

namespace core::log::cat {

    /**
     * @brief Базовые категории логирования.
     *
     * Идея:
     *  - каждая подсистема (ресурсы, ECS, UI, ИИ и т.д.) пишет в свою категорию;
     *  - в лог-файлах это позволяет быстро отфильтровать нужные записи;
     *  - при необходимости набор можно расширять, но эти имена — "ядро".
     */

    // Разовые отладочные сообщения, которые не попадают в другие категории.
    inline constexpr std::string_view Debug = "Debug";

    // Общая инфраструктура движка, bootstrapping, жизненный цикл приложения.
    inline constexpr std::string_view Engine    = "Engine";

    // Парсинг и валидация JSON-конфигов, настройка окон/игровых параметров.
    inline constexpr std::string_view Config    = "Config";

    // Загрузка и управление ресурсами: текстуры, звуки, шрифты.
    inline constexpr std::string_view Resources = "Resources";

    // ECS-ядро: сущности, системы, компоненты, внутренние структуры.
    inline constexpr std::string_view ECS       = "ECS";

    // Отрисовка: рендеринг, камеры, viewport, post-processing и т.п.
    inline constexpr std::string_view Rendering = "Rendering";

    // Аудио-подсистема.
    inline constexpr std::string_view Audio     = "Audio";

    // Ввод: клавиатура, мышь, геймпады и т.п.
    inline constexpr std::string_view Input     = "Input";

    // Интерфейс (UI): якоря, масштабирование, окна, HUD.
    inline constexpr std::string_view UI        = "UI";

    // Игровая логика: SkyGuard, будущая 4X, события и т.п.
    inline constexpr std::string_view Gameplay  = "Gameplay";

    // Искусственный интеллект.
    inline constexpr std::string_view AI        = "AI";

    // Сетевой код.
    inline constexpr std::string_view Network   = "Network";

    // Сохранение/загрузка игр (save/load subsystem).
    inline constexpr std::string_view SaveLoad  = "SaveLoad";

    // Сообщения, которые явно адресованы игроку (диалоги, предупреждения).
    inline constexpr std::string_view User      = "User";

    // Производительность: таймеры, профилирование, время кадра/загрузки.
    inline constexpr std::string_view Performance = "Performance";

} // namespace core::log::cat