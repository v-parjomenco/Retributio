// ================================================================================================
// File: tests/engine/include/test_panic.h
// Purpose: expectPanic<F>() — the sole mechanism for asserting panic paths in test binaries
//          that link log_panic_sink_throw.cpp.
//
// Architectural contract:
//   log_panic_sink_throw.cpp overrides core::log::detail::panic_sink to throw
//   std::runtime_error instead of calling std::exit(). This enables testing panic paths
//   without terminating the process and allows asserting on the error message content.
//
//   ASSERT_DEATH / EXPECT_DEATH are FORBIDDEN in any binary that links
//   log_panic_sink_throw.cpp. GTest runs death tests in a forked child process and
//   expects it to terminate (exit/abort). If an exception escapes instead,
//   GTest reports "threw an exception" and marks the test as FAILED.
//
//   The only correct way to assert a panic in this test environment:
//     const std::string msg = expectPanic([&]{ /* operation that panics */ });
//     EXPECT_NE(msg.find("marker"), std::string::npos) << msg;
//
// Usage:
//   #include "test_panic.h"
//   using test_support::expectPanic;
//
//   // Assert that a panic occurred:
//   const std::string msg = expectPanic([&]{ registry.loadFromSources(sources); });
//
//   // Assert a structured marker in the message:
//   EXPECT_NE(msg.find("[RR-INVALID-KEY]"), std::string::npos) << "Panic msg: " << msg;
//
//   // Assert the subsystem name (when no structured markers are defined):
//   EXPECT_NE(msg.find("ResourceManager"), std::string::npos) << "Panic msg: " << msg;
//
// Include ONLY from TUs compiled into test binaries that link log_panic_sink_throw.cpp.
// Never include from production code.
// ================================================================================================
#pragma once

#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

namespace test_support {

    // --------------------------------------------------------------------------------------------
    // expectPanic<F>
    //
    // Вызывает fn() и ожидает std::runtime_error от panic_sink.
    // Возвращает what() пойманного исключения для дальнейшей проверки содержимого.
    // Если исключение не было брошено — вызывает ADD_FAILURE(), тест помечается как упавший,
    // возвращается пустая строка.
    //
    // Шаблон без type-erasure: нет std::function, нет heap-аллокации, нет overhead.
    // [[nodiscard]]: предупреждение компилятора если результат молча отброшен.
    // --------------------------------------------------------------------------------------------
    template <class F>
    [[nodiscard]] std::string expectPanic(F&& fn) {
        try {
            std::forward<F>(fn)();
        } catch (const std::runtime_error& e) {
            return e.what();
        }
        ADD_FAILURE()
            << "Expected a panic (std::runtime_error from panic_sink) — "
               "but no exception was thrown. "
               "Check that the operation actually reaches a LOG_PANIC / LOG_CRITICAL path.";
        return {};
    }

} // namespace test_support
