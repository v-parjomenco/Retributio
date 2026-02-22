#include "pch.h"

#include "core/log/detail/utf8_to_wide.h"
#include "core/log/logging.h"

#include <cstdlib>
#include <format>
#include <iostream>
#include <string>
#include <string_view>

#ifdef _WIN32
    #include <windows.h>
#endif

namespace core::log::detail {

    [[noreturn]] void panic_sink(std::string_view category,
                                  std::string_view message,
                                  const char* file,
                                  int line) {
        // Crash-path: форматирование через std::format допустимо — это не hot path.
        const std::string userMessage = std::format(
            "Категория: {}\n\n{}\n\nФайл: {}\nСтрока: {}",
            category,
            message,
            file != nullptr ? file : "",
            line);

#ifdef _WIN32
        const std::wstring wide = utf8ToWide(userMessage);
        MessageBoxW(nullptr,
                    wide.c_str(),
                    L"Критическая ошибка",
                    MB_OK | MB_ICONERROR | MB_SETFOREGROUND | MB_TOPMOST);
#else
        std::cerr << "\n========================================\n"
                  << "КРИТИЧЕСКАЯ ОШИБКА\n\n"
                  << userMessage << '\n'
                  << "========================================\n\n";
#endif

        std::exit(EXIT_FAILURE);
    }

} // namespace core::log::detail