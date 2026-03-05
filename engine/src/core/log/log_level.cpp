#include "pch.h"

#include "core/log/log_level.h"

using namespace std::literals;

namespace core::log {

    std::string_view toString(Level level) noexcept {
        switch (level) {
        case Level::Trace:    return "TRACE"sv;
        case Level::Debug:    return "DEBUG"sv;
        case Level::Info:     return "INFO"sv;
        case Level::Warning:  return "WARNING"sv;
        case Level::Error:    return "ERROR"sv;
        case Level::Critical: return "CRITICAL"sv;
        }

        // На случай некорректного значения (не должен происходить, но безопасно иметь fallback).
        return "UNKNOWN"sv;
    }

} // namespace core::log