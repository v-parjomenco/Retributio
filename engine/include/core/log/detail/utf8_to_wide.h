// ================================================================================================
// File: core/log/detail/utf8_to_wide.h
// Purpose: Win32-only UTF-8 -> UTF-16 conversion helper for logging sinks.
// ================================================================================================
#pragma once

#include <string>
#include <string_view>

namespace core::log::detail {

#ifdef _WIN32
    /// @brief Конвертирует UTF-8 строку в UTF-16 (std::wstring) для Win32 API.
    ///
    /// При пустой строке возвращает пустой wstring без вызова Win32 API.
    ///
    /// При ошибке (невалидный UTF-8, size > INT_MAX или сбой Win32 API) возвращает
    /// диагностическую строку формата `L"<utf8ToWide failed: err=0x????????>"`,
    /// где код — GetLastError() сразу после сбоя. Это позволяет caller-у (panic_sink,
    /// terminate handler, user_dialog) всегда вывести осмысленное сообщение,
    /// а не пустое или молча подменённое.
    ///
    /// @note Использует MB_ERR_INVALID_CHARS: невалидные UTF-8 последовательности
    ///       вызывают явный failure, а не тихую подстановку U+FFFD.
    [[nodiscard]] std::wstring utf8ToWide(std::string_view text);
#endif // _WIN32

} // namespace core::log::detail