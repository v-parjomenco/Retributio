#include "core/runtime/entry/terminate_handler.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <exception>

#ifdef _WIN32
    #include <windows.h>
#endif

#include "core/log/detail/utf8_to_wide.h"
#include "core/log/log_macros.h"
#include "core/log/logging.h"

namespace core::runtime::entry {

namespace {

    // Защита от рекурсии: если LOG_PANIC → panic_sink → terminate → handler повторно.
    // acquire/release: exchange публикует флаг и видит чужую запись без гонки.
    std::atomic<bool> g_terminating{false};

    // Безопасный вывод до инициализации логгера: stderr + OutputDebugString (Windows).
    // noexcept: fallback path не должен сам упасть с исключением.
    void fallbackReport(const char* message) noexcept {
        std::fprintf(stderr, "[FATAL] %s\n", message);
        std::fflush(stderr);

#ifdef _WIN32
        // Дублируем в debugger output — видно в Visual Studio Output window.
        try {
            const std::wstring wide = core::log::detail::utf8ToWide(message);
            OutputDebugStringW(L"[FATAL] ");
            OutputDebugStringW(wide.c_str());
            OutputDebugStringW(L"\n");
        } catch (...) {
            // utf8ToWide не должна бросать, но terminate-path должен быть bulletproof.
            OutputDebugStringW(
                L"[FATAL] <UTF-8 conversion failed in terminate handler>\n");
        }
#endif
    }

    // Основной terminate handler. Никогда не возвращает управление.
    [[noreturn]] void terminateHandler() {
        // Рекурсивный вызов terminate (например, если LOG_PANIC → panic_sink →
        // terminate): жёстко уходим через abort, не допуская бесконечной рекурсии.
        if (g_terminating.exchange(true, std::memory_order_acq_rel)) {
            std::abort();
        }

        try {
            if (core::log::isInitialized()) {
                // Логгер активен — используем полный crash path с гарантированным flush.
                const auto ex = std::current_exception();
                if (ex) {
                    try {
                        std::rethrow_exception(ex);
                    } catch (const std::exception& e) {
                        // LOG_PANIC [[noreturn]]: вызывает panic_sink → exit. Сюда не возвращаемся.
                        LOG_PANIC(core::log::cat::Engine,
                                  "std::terminate: необработанное исключение: {}",
                                  e.what());
                    } catch (...) {
                        LOG_PANIC(core::log::cat::Engine,
                                  "std::terminate: необработанное исключение неизвестного типа");
                    }
                } else {
                    LOG_PANIC(core::log::cat::Engine,
                              "std::terminate вызван без активного исключения");
                }
            } else {
                // Логгер ещё не инициализирован — безопасный fallback в stderr/debugger.
                // LOG_PANIC использовать нельзя: panic_sink делает MessageBox + flush логгера.
                const auto ex = std::current_exception();
                if (ex) {
                    try {
                        std::rethrow_exception(ex);
                    } catch (const std::exception& e) {
                        fallbackReport(e.what());
                    } catch (...) {
                        fallbackReport(
                            "std::terminate: исключение неизвестного типа (до init логгера)");
                    }
                } else {
                    fallbackReport(
                        "std::terminate вызван без активного исключения (до init логгера)");
                }
                // Fallback path завершает процесс через abort — нет panic_sink на этом этапе.
                std::abort();
            }
        } catch (...) {
            // Если handler сам упал — жёсткий abort без рекурсии в catch.
            std::abort();
        }
    }

} // namespace

    void installTerminateHandler() noexcept {
        std::set_terminate(terminateHandler);
    }

} // namespace core::runtime::entry