// ================================================================================================
// File: game/skyguard/utils/debug_format.h
// Purpose: Zero-allocation debug formatting helpers for SkyGuard
// ================================================================================================
#pragma once

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

#include "game/skyguard/presentation/background_renderer.h"

namespace game::skyguard::utils {

    namespace detail {

        inline bool appendLiteral(char*& it, char* end, std::string_view text) noexcept {
            const std::size_t remaining = static_cast<std::size_t>(end - it);
            if (text.size() > remaining) {
                return false;
            }
            std::memcpy(it, text.data(), text.size());
            it += text.size();
            return true;
        }

        template <typename T>
        inline bool appendNumber(char*& it, char* end, T value) noexcept {
            auto [ptr, ec] = std::to_chars(it, end, value);
            if (ec != std::errc{}) {
                return false;
            }
            it = ptr;
            return true;
        }

    } // namespace detail

    [[nodiscard]] inline std::size_t formatBackgroundStatsLine(
        char* buf,
        std::size_t cap,
        const presentation::BackgroundRenderer::BackgroundStats& stats) noexcept {
        if (cap == 0) {
            return 0;
        }

        char* it = buf;
        char* end = buf + cap;

        if (!detail::appendLiteral(it, end, "Background: tiles ")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, stats.tilesCovered)) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, "  draws ")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, stats.drawCalls)) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, "  tile ")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, stats.tileSize.x)) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, "x")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, stats.tileSize.y)) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, "  view ")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end,
                                  static_cast<std::uint32_t>(stats.visibleRect.size.x))) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, "x")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end,
                                  static_cast<std::uint32_t>(stats.visibleRect.size.y))) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, "  pos ")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end,
                                  static_cast<std::int32_t>(stats.visibleRect.position.x))) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, ",")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end,
                                  static_cast<std::int32_t>(stats.visibleRect.position.y))) {
            return static_cast<std::size_t>(it - buf);
        }

        return static_cast<std::size_t>(it - buf);
    }

} // namespace game::skyguard::utils