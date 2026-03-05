#include "pch.h"

#include "core/log/detail/utf8_to_wide.h"

#ifdef _WIN32
    #include <cwchar>
    #include <limits>
    #include <windows.h>
#endif

namespace core::log::detail {

#ifdef _WIN32

    std::wstring utf8ToWide(std::string_view text) {
        if (text.empty()) {
            return {};
        }

        // Защита от size_t → int overflow: на практике лог-сообщения никогда не достигают
        // INT_MAX байт, но корректный код не должен полагаться на это.
        if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            return L"<utf8ToWide failed: err=0x00000000 (input too large)>";
        }

        const int inputLen = static_cast<int>(text.size());

        // MB_ERR_INVALID_CHARS: делает невалидные UTF-8 последовательности явной ошибкой.
        // Без этого флага MultiByteToWideChar молча подставляет U+FFFD, и caller получает
        // молча искажённое сообщение без какого-либо сигнала о проблеме.
        const int required = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            text.data(),
            inputLen,
            nullptr,
            0);

        if (required <= 0) {
            // GetLastError() вызывается немедленно: любой Win32-вызов между ним и
            // MultiByteToWideChar перезаписал бы код ошибки потока.
            const DWORD err = GetLastError();
            wchar_t buf[48];
            swprintf_s(buf, L"<utf8ToWide failed: err=0x%08X>",
                       static_cast<unsigned>(err));
            return buf;
        }

        std::wstring result;
        result.resize(static_cast<std::size_t>(required));

        const int converted = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            text.data(),
            inputLen,
            result.data(),
            required);

        if (converted <= 0) {
            const DWORD err = GetLastError();
            wchar_t buf[48];
            swprintf_s(buf, L"<utf8ToWide failed: err=0x%08X>",
                       static_cast<unsigned>(err));
            return buf;
        }

        return result;
    }

#endif // _WIN32

} // namespace core::log::detail