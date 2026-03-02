#include "pch.h"

#include "core/runtime/entry/user_dialog.h"

#include <cstdio>

#ifdef _WIN32
    #include <windows.h>
#endif

#include "core/debug/debug_config.h"
#include "core/log/detail/utf8_to_wide.h"

namespace core::runtime::entry {

    void showError(std::string_view text) noexcept {
#ifdef _WIN32
        try {
            // utf8ToWide возвращает заглушку при ошибке конвертации — MessageBox всё равно
            // откроется, пусть и с нечитаемым текстом. Это лучше, чем тихий провал.
            const std::wstring wideText  = core::log::detail::utf8ToWide(text);
            MessageBoxW(
                nullptr,
                wideText.c_str(),
                L"Error",
                MB_OK | MB_ICONERROR | MB_SETFOREGROUND | MB_TOPMOST);
        } catch (...) {
            // MessageBoxW теоретически может дать исключение через std::wstring —
            // падаем обратно в stderr чтобы не потерять сообщение об ошибке.
            std::fprintf(stderr,
                         "[Error] %.*s\n",
                         static_cast<int>(text.size()),
                         text.data());
        }
#else
        std::fprintf(stderr,
                     "[Error] %.*s\n",
                     static_cast<int>(text.size()),
                     text.data());
        std::fflush(stderr);
#endif
    }

    void holdOnExitIfEnabled() noexcept {
        // if constexpr: ветка полностью вырезается компилятором в Release/Profile.
        if constexpr (core::debug::DEBUG_HOLD_ON_EXIT) {
#ifdef _WIN32
            MessageBoxW(
                nullptr,
                L"Press OK to exit.",
                L"Exit",
                MB_OK | MB_SETFOREGROUND);
#else
            std::fprintf(stdout, "\nPress Enter to exit...\n");
            std::fflush(stdout);
            // getchar() ждёт Enter. Если stdin закрыт — немедленно возвращает EOF, что безопасно.
            std::getchar();
#endif
        }
    }

} // namespace core::runtime::entry