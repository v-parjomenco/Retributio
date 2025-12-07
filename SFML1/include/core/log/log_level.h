// ================================================================================================
// File: core/log/log_level.h
// Purpose: Defines logging severity levels for the engine-wide logging subsystem.
// Used by: core/log/logging.h, core/log/log_macros.h, diagnostics tools.
// ================================================================================================
#pragma once

#include <cstdint>
#include <string_view>

namespace core::log {

    /**
     * @brief Уровни важности сообщений логирования.
     *
     * Иерархия (от "самый болтливый" к "самый важный"):
     *  - Trace    — пошаговая трассировка, профилирование, внутренние детали;
     *  - Debug    — отладочная информация, полезная при разработке;
     *  - Info     — обычные события: запуск/остановка систем, загрузка ресурсов;
     *  - Warning  — потенциальная проблема, но игра продолжает работать;
     *  - Error    — ошибка: часть функциональности не работает, но игра живёт;
     *  - Critical — фатальная ошибка: дальше работать нельзя, требуется остановка.
     *
     * В Debug-сборках часто включают Trace/Debug, в Release — только Info и выше.
     */
    enum class Level : std::uint8_t {
        Trace    = 0,
        Debug    = 1,
        Info     = 2,
        Warning  = 3,
        Error    = 4,
        Critical = 5
    };

    /**
     * @brief Возвращает человеко-читаемое имя уровня логирования.
     *
     * Примеры:
     *  - Level::Info     -> "INFO"
     *  - Level::Warning  -> "WARNING"
     *
     * Реализация находится в .cpp, чтобы не плодить зависимости в заголовке.
     */
    std::string_view toString(Level level) noexcept;

} // namespace core::log