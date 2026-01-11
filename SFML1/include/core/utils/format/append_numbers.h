// ================================================================================================
// File: core/utils/format/append_numbers.h
// Purpose: Header-only integer formatting helpers for zero-allocation string appends
// ================================================================================================
#pragma once

#include <array>
#include <charconv>
#include <cstdint>
#include <string>
#include <system_error>

namespace core::utils::format {

    inline void appendU64(std::string& out, std::uint64_t value) {
        std::array<char, 32> buf{};
        auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);
        if (ec == std::errc{}) {
            out.append(buf.data(), static_cast<std::size_t>(ptr - buf.data()));
        } else {
            out.append("?");
        }
    }

    inline void appendU32(std::string& out, std::uint32_t value) {
        appendU64(out, static_cast<std::uint64_t>(value));
    }

    inline void appendI32(std::string& out, std::int32_t value) {
        std::array<char, 32> buf{};
        auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);
        if (ec == std::errc{}) {
            out.append(buf.data(), static_cast<std::size_t>(ptr - buf.data()));
        } else {
            out.append("?");
        }
    }

    inline void appendMs1DecimalFromUs(std::string& out, std::uint64_t us) {
        // Печатаем миллисекунды с 1 знаком после точки без float:
        // ms10 = (us / 100) == 0.1ms units (с округлением).
        const std::uint64_t ms10 = (us + 50) / 100;

        const std::uint64_t intPart = ms10 / 10;
        const std::uint64_t frac = ms10 % 10;

        appendU64(out, intPart);
        out.push_back('.');
        out.push_back(static_cast<char>('0' + static_cast<int>(frac)));
    }

} // namespace core::utils::format