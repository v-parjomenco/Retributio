// ================================================================================================
// File: dev/stress_env_utils.h
// Purpose: Shared env-reading utilities for Profile-only stress infrastructure.
//          Consolidates duplicated helpers from stress_render_options.cpp,
//          stress_render_chunk_content_provider.cpp, spatial_v2_config_builder.cpp.
// ================================================================================================
#pragma once

#if defined(RETRIBUTIO_PROFILE)

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string_view>
#include <system_error>

#include "core/log/log_macros.h"

namespace game::atrapacielos::dev::env {

    // --------------------------------------------------------------------------------------------
    // readEnvStringView: safe cross-platform getenv into caller-owned buffer.
    // Returns empty view if variable is unset, empty, or exceeds buffer.
    // --------------------------------------------------------------------------------------------
    template <std::size_t N>
    [[nodiscard]] inline std::string_view readStringView(const char* name,
                                                         std::array<char, N>& buf) noexcept {
        static_assert(N > 1, "Buffer must have room for null terminator.");

#ifdef _WIN32
        std::size_t required = 0;
        if (::getenv_s(&required, buf.data(), buf.size(), name) != 0) {
            return {};
        }
        if (required == 0 || required > buf.size()) {
            return {};
        }
        return std::string_view{buf.data(), required - 1};
#else
        const char* s = std::getenv(name);
        if (s == nullptr || *s == '\0') {
            return {};
        }
        return std::string_view{s};
#endif
    }

    // --------------------------------------------------------------------------------------------
    // readBool: "0" or empty/unset → false, anything else → true.
    // --------------------------------------------------------------------------------------------
    [[nodiscard]] inline bool readBool(const char* name) noexcept {
#ifdef _WIN32
        std::size_t required = 0;
        if (::getenv_s(&required, nullptr, 0, name) != 0) {
            return false;
        }
        if (required <= 1) {
            return false;
        }
        std::array<char, 8> buf{};
        if (::getenv_s(&required, buf.data(), buf.size(), name) != 0) {
            return false;
        }
        if (required <= 1 || required > buf.size()) {
            return false;
        }
        return buf[0] != '0';
#else
        const char* s = std::getenv(name);
        return s != nullptr && *s != '\0' && *s != '0';
#endif
    }

    // --------------------------------------------------------------------------------------------
    // parseSizeOrDefault: from_chars with range clamp.
    // --------------------------------------------------------------------------------------------
    [[nodiscard]] inline std::size_t parseSizeOrDefault(std::string_view s,
                                                        const std::size_t defaultValue,
                                                        const std::size_t maxValue) noexcept {
        if (s.empty()) {
            return defaultValue;
        }
        std::uint64_t value = 0;
        const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
        if (ec != std::errc{} || ptr != (s.data() + s.size())) {
            return defaultValue;
        }
        if (value > static_cast<std::uint64_t>(maxValue)) {
            return maxValue;
        }
        return static_cast<std::size_t>(value);
    }

    // --------------------------------------------------------------------------------------------
    // readSize: env → size_t with default/max clamp.
    // --------------------------------------------------------------------------------------------
    [[nodiscard]] inline std::size_t readSize(const char* name,
                                              const std::size_t defaultValue,
                                              const std::size_t maxValue) noexcept {
        std::array<char, 64> buf{};
        return parseSizeOrDefault(readStringView(name, buf), defaultValue, maxValue);
    }

    // --------------------------------------------------------------------------------------------
    // readU64 / readU32: env → exact integer with overflow detection.
    // --------------------------------------------------------------------------------------------
    [[nodiscard]] inline std::optional<std::uint64_t> readU64(const char* name) noexcept {
#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable : 4996) // getenv
#endif
        const char* s = std::getenv(name);
#if defined(_MSC_VER)
    #pragma warning(pop)
#endif
        if (s == nullptr || *s == '\0') {
            return std::nullopt;
        }
        std::uint64_t value = 0;
        const char* end = s;
        while (*end != '\0') {
            ++end;
        }
        const auto [ptr, ec] = std::from_chars(s, end, value);
        if (ec != std::errc{} || ptr != end) {
            return std::nullopt;
        }
        return value;
    }

    [[nodiscard]] inline std::optional<std::uint32_t> readU32(const char* name) noexcept {
        const auto v64 = readU64(name);
        if (!v64.has_value()) {
            return std::nullopt;
        }
        if (*v64 > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
            LOG_PANIC(core::log::cat::ECS,
                      "stress_env_utils: env {}={} exceeds uint32 range", name, *v64);
        }
        return static_cast<std::uint32_t>(*v64);
    }

    // --------------------------------------------------------------------------------------------
    // trimAsciiSpacesTabs: strip leading/trailing whitespace (space+tab only).
    // --------------------------------------------------------------------------------------------
    [[nodiscard]] inline std::string_view trimAsciiSpacesTabs(std::string_view s) noexcept {
        while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
            s.remove_prefix(1);
        }
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) {
            s.remove_suffix(1);
        }
        return s;
    }

    // --------------------------------------------------------------------------------------------
    // parseCsvTokens: split by comma, trim each, call visitor. Stop when visitor returns false.
    // --------------------------------------------------------------------------------------------
    template <typename Visitor>
    inline void parseCsvTokens(std::string_view csv, Visitor&& visitor) noexcept {
        while (!csv.empty()) {
            const std::size_t comma = csv.find(',');
            std::string_view token =
                (comma == std::string_view::npos) ? csv : csv.substr(0, comma);
            token = trimAsciiSpacesTabs(token);
            if (!token.empty()) {
                if (!visitor(token)) {
                    return;
                }
            }
            if (comma == std::string_view::npos) {
                return;
            }
            csv.remove_prefix(comma + 1);
        }
    }

    // --------------------------------------------------------------------------------------------
    // parseU32: strict from_chars for a single uint32 token.
    // --------------------------------------------------------------------------------------------
    [[nodiscard]] inline bool parseU32(std::string_view s, std::uint32_t& out) noexcept {
        s = trimAsciiSpacesTabs(s);
        if (s.empty()) {
            return false;
        }
        std::uint32_t value = 0;
        const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
        if (ec != std::errc{} || ptr != (s.data() + s.size())) {
            return false;
        }
        out = value;
        return true;
    }

} // namespace game::atrapacielos::dev::env

#endif // defined(RETRIBUTIO_PROFILE)