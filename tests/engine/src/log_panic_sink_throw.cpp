// ================================================================================================
// File: tests/engine/src/log_panic_sink_throw.cpp
// Purpose: Test-only definition of core::log::detail::panic_sink.
//          Throws std::runtime_error instead of exiting the process, allowing tests to
//          assert on panic paths via expectPanicMessage() / EXPECT_THROW.
// Used by: retributio_engine_tests
// Related headers: core/log/logging.h
// Notes:
//  - This file satisfies the linker requirement for panic_sink that retributio_core declares
//    but does not define. Must be linked ONLY into targets that do NOT link retributio_runtime;
//    linking both causes a multiple-definition error.
// ================================================================================================
#include "core/log/logging.h"

#include <charconv>
#include <stdexcept>
#include <string>
#include <string_view>

namespace core::log::detail {

    [[noreturn]] void panic_sink(std::string_view category,
                                  std::string_view message,
                                  const char* file,
                                  int line) {
        std::string text;
        text.reserve(256);
        text += "category=";
        text.append(category.data(), category.size());
        text += " message=";
        text.append(message.data(), message.size());
        text += " file=";
        if (file != nullptr) {
            text += file;
        }
        text += " line=";
        char lineBuf[16];
        const auto [ptr, ec] = std::to_chars(lineBuf, lineBuf + sizeof(lineBuf), line);
        text.append(lineBuf, ec == std::errc{} ? ptr : lineBuf);

        throw std::runtime_error(text);
    }

} // namespace core::log::detail