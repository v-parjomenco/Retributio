#pragma once
#include <string>
#include <iostream>
#include "../core/config.h" // подключаем флаг
#ifdef _WIN32
    #include <windows.h>
#endif

namespace utils {
    namespace message { // добавляем вложенное пространство имён message

        // --------------------------------------------------------
        // Показывает окно или сообщение об ошибке (зависит от ОС)
        // --------------------------------------------------------
        inline void showError(const std::string& message) {
            #ifdef _WIN32
            MessageBoxA(nullptr, message.c_str(), "Ошибка", MB_OK | MB_ICONERROR);
            #else
            std::cerr << "Ошибка: " << message << std::endl;
            #endif
        }

        // --------------------------------------------------------
        // Показывает информационное окно (или сообщение в консоль)
        // --------------------------------------------------------
        inline void showInfo(const std::string& message) {
            #ifdef _WIN32
            MessageBoxA(nullptr, message.c_str(), "Информация", MB_OK | MB_ICONINFORMATION);
            #else
            std::cout << "Информация: " << message << std::endl;
            #endif
        }

        // --------------------------------------------------------
        // Показывает отладочное сообщение (только в Debug-сборке)
        // --------------------------------------------------------
        inline void logDebug(const std::string& message) {
            #ifdef _DEBUG
                #ifdef _WIN32
                MessageBoxA(nullptr, message.c_str(), "Debug", MB_OK | MB_ICONINFORMATION);
            #else
                std::cout << "[Debug] " << message << std::endl;
                #endif
            #endif
        }

        // --------------------------------------------------------
        // Задержка перед выходом (используется при DEBUG_HOLD_ON_EXIT)
        // --------------------------------------------------------
        static void holdOnExit() {
            if constexpr (config::DEBUG_HOLD_ON_EXIT) {
            #ifdef _WIN32
                MessageBoxA(nullptr, "Нажмите OK, чтобы выйти.", "Завершение", MB_OK);
            #else
                std::cout << "\nНажмите Enter, чтобы выйти..." << std::endl;
                std::cin.get();
            #endif
            }
        }

    } // namespace message
} // namespace utils
