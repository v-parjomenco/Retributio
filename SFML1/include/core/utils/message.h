#pragma once

#include <iostream>
#include <stdexcept>
#include <string>

#ifdef _WIN32
    #ifndef NOMINMAX        // предотвращает конфликт с std::min / std::max
        #define NOMINMAX
    #endif
    #include <windows.h>
#endif

#include "core/config.h"

namespace core::utils {
#ifdef _DEBUG
    #ifndef _WIN32
    // Цвета ANSI для консоли (Linux / macOS)
    namespace ansi {
        constexpr const char* reset = "\033[0m";
        constexpr const char* red = "\033[31m";
        constexpr const char* green = "\033[32m";
        constexpr const char* yellow = "\033[33m";
        constexpr const char* blue = "\033[34m";
        constexpr const char* cyan = "\033[36m";
    }
    #endif
#endif

    // -----------------------------------------------------------------------------
    // Конвертация в нативные форматы строк для Windows (UTF-8 → UTF-16)
    // -----------------------------------------------------------------------------

#ifdef _WIN32
    inline std::wstring utf8ToNative(const std::string& str) {
        if (str.empty()) return L"";

        int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            str.c_str(), -1, nullptr, 0);
        if (required == 0)
            throw std::runtime_error("Ошибка конвертации UTF-8 → UTF-16");

        std::wstring result(required, L'\0');
        int converted = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            str.c_str(), -1, result.data(), required);
        if (converted == 0)
            throw std::runtime_error ("[Ошибка конвертации UTF-8 → UTF-16]");

        if (!result.empty() && result.back() == L'\0')
            result.pop_back();

        return result;
    }

    inline void showMessageBox(const std::string& text, const std::string& title, UINT type) {
        MessageBoxW(nullptr, utf8ToNative(text).c_str(), utf8ToNative(title).c_str(), static_cast<UINT>(type));
    }
#else
    inline void showMessageBox(const std::string& text, const std::string& title, int /*type*/) {
        std::cout << "[" << title << "] " << text << std::endl;
    }
#endif

    // -----------------------------------------------------------------------------
    // Пространство имён для удобных вызовов сообщений
    // -----------------------------------------------------------------------------

    namespace message {

        inline void showError(const std::string& message) {
#ifdef _WIN32
            showMessageBox(message, "Ошибка", MB_OK | MB_ICONERROR);
#else
            showMessageBox(message, "Ошибка", 0);
#endif
        }

        inline void showInfo(const std::string& message) {
#ifdef _WIN32
            showMessageBox(message, "Информация", MB_OK | MB_ICONINFORMATION);
#else
            showMessageBox(message, "Информация", 0);
#endif
        }

        inline void logDebug(const std::string& message) {
#ifdef _DEBUG
    #ifdef _WIN32
            showMessageBox(message, "Отладка", MB_OK | MB_ICONINFORMATION);
    #else
            // Цветной вывод для Linux/macOS
            std::cout
                << ansi::cyan << "[Отладка] "
                << ansi::blue << message
                << ansi::reset << std::endl;
    #endif
#endif
        }

        // -----------------------------------------------------------------------
        // Умный макрос отладки: добавляет имя файла и номер строки автоматически
        // Работает только в Debug, в Release полностью вырезается
        // -----------------------------------------------------------------------

#ifdef _DEBUG
    #define DEBUG_MSG(msg) \
        ::core::utils::message::logDebug( \
            std::string("[") + __FILE__ + ":" + std::to_string(__LINE__) + "] " + msg \
        )
#else
    #define DEBUG_MSG(msg) ((void)0)
#endif

        inline void holdOnExit() {
            if constexpr (::config::DEBUG_HOLD_ON_EXIT) {
#ifdef _WIN32
                showMessageBox("Нажмите OK, чтобы выйти.", "Завершение", MB_OK);
#else
                std::cout << "\nНажмите Enter, чтобы выйти..." << std::endl;
                std::cin.get();
#endif
            }
        }
    } // namespace message
} // namespace core::utils