// ================================================================================================
// File: core/log/detail/utf8_to_wide.h
// Purpose: Win32-only UTF-8 -> UTF-16 conversion helper for logging sinks.
// ================================================================================================
#pragma once

#include <string>
#include <string_view>

namespace core::log::detail {

#ifdef _WIN32

    /// @brief Конвертирует UTF-8 строку в UTF-16 (wstring) для Win32 API.
    /// @return Сконвертированная строка, или заглушка при ошибке конвертации.
    [[nodiscard]] std::wstring utf8ToWide(std::string_view text);

#endif // _WIN32

} // namespace core::log::detail