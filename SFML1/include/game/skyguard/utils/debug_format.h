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

        // Форматирование float-числа в фиксированном формате с 2 знаками после запятой,
        // без зависимости от std::to_chars(float).
        inline bool appendFixed2(char*& it, char* end, float value) noexcept {
            const float scaledF = value * 100.0f;
            const float adj = (scaledF >= 0.0f) ? 0.5f : -0.5f;
            const std::int64_t scaled = static_cast<std::int64_t>(scaledF + adj);

            const std::int64_t intPart = scaled / 100;
            const std::int64_t fracAbs = (scaled >= 0) ? (scaled % 100) : (-(scaled % 100));

            if (!appendNumber(it, end, intPart)) {
                return false;
            }
            if (it >= end) {
                return false;
            }
            *it++ = '.';
            if (end - it < 2) {
                return false;
            }

            const char d1 = static_cast<char>('0' + static_cast<int>((fracAbs / 10) % 10));
            const char d2 = static_cast<char>('0' + static_cast<int>(fracAbs % 10));
            it[0] = d1;
            it[1] = d2;
            it += 2;
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

    [[nodiscard]] inline std::size_t formatCameraStatsLine(char* buf, std::size_t cap,
                                                           float playerY, float viewCenterY,
                                                           float viewSizeY, float cameraOffsetY,
                                                           float cameraCenterYMax) noexcept {

        if (cap == 0) {
            return 0;
        }

        char* it = buf;
        char* end = buf + cap;

        if (!detail::appendLiteral(it, end, "Camera: playerY "))
            return static_cast<std::size_t>(it - buf);
        if (!detail::appendFixed2(it, end, playerY))
            return static_cast<std::size_t>(it - buf);

        if (!detail::appendLiteral(it, end, "  centerY "))
            return static_cast<std::size_t>(it - buf);
        if (!detail::appendFixed2(it, end, viewCenterY))
            return static_cast<std::size_t>(it - buf);

        if (!detail::appendLiteral(it, end, "  sizeY "))
            return static_cast<std::size_t>(it - buf);
        if (!detail::appendFixed2(it, end, viewSizeY))
            return static_cast<std::size_t>(it - buf);

        if (!detail::appendLiteral(it, end, "  offsetY "))
            return static_cast<std::size_t>(it - buf);
        if (!detail::appendFixed2(it, end, cameraOffsetY))
            return static_cast<std::size_t>(it - buf);

        if (!detail::appendLiteral(it, end, "  maxY "))
            return static_cast<std::size_t>(it - buf);
        if (!detail::appendFixed2(it, end, cameraCenterYMax))
            return static_cast<std::size_t>(it - buf);

        return static_cast<std::size_t>(it - buf);
    }

} // namespace game::skyguard::utils