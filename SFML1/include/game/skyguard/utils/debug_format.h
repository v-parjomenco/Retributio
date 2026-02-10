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
#include <system_error>

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
    #include <SFML/Graphics/View.hpp>
    #include <SFML/System/Vector2.hpp>
    #include "core/spatial/aabb2.h"
    #include "core/spatial/chunk_coord.h"
    #include "core/spatial/spatial_index_v2.h"
#endif

namespace game::skyguard::presentation {
    class BackgroundRenderer;
}

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

        template <typename T> inline bool appendNumber(char*& it, char* end, T value) noexcept {
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

        inline bool appendFixed1(char*& it, char* end, float value) noexcept {
            const float scaledF = value * 10.0f;
            const float adj = (scaledF >= 0.0f) ? 0.5f : -0.5f;
            const std::int64_t scaled = static_cast<std::int64_t>(scaledF + adj);

            const std::int64_t intPart = scaled / 10;
            const std::int64_t fracAbs = (scaled >= 0) ? (scaled % 10) : (-(scaled % 10));

            if (!appendNumber(it, end, intPart)) {
                return false;
            }
            if (it >= end) {
                return false;
            }
            *it++ = '.';
            if (it >= end) {
                return false;
            }

            const char d1 = static_cast<char>('0' + static_cast<int>(fracAbs % 10));
            *it++ = d1;
            return true;
        }

    } // namespace detail

#if !defined(NDEBUG) || defined(SFML1_PROFILE)

    [[nodiscard]] std::size_t
    formatBackgroundStatsLine(char* buf, std::size_t cap,
                              const presentation::BackgroundRenderer& backgroundRenderer) noexcept;

    [[nodiscard]] std::size_t
    formatStreamingStatsLine(char* buf, std::size_t cap,
                             const sf::View& view,
                             core::spatial::ChunkCoord windowOrigin,
                             std::int32_t chunkSizeWorld) noexcept;

    [[nodiscard]] std::size_t
    formatCellHealthLine(char* buf, std::size_t cap,
                         std::uint32_t maxLen,
                         std::uint32_t sumLen,
                         std::uint32_t dupApprox,
                         std::uint32_t loaded) noexcept;

    [[nodiscard]] std::size_t
    formatPlayerWatchStateLine(char* buf, std::size_t cap,
                               std::uint64_t entityId,
                               bool hasSpatialId,
                               std::uint32_t spatialId,
                               bool hasDirty,
                               bool hasStreamedOut,
                               const sf::Vector2f& position) noexcept;

    // ВАЖНО:
    // eligible != "реально отрисовано".
    // eligible = "по известным нам условиям в UI-pass объект должен был попасть в render-pass".
    [[nodiscard]] std::size_t
    formatPlayerWatchVisibilityLine(char* buf, std::size_t cap,
                                    const core::spatial::Aabb2& lastAabb,
                                    const core::spatial::Aabb2& newAabb,
                                    bool fineCullPass,
                                    bool inQuery,
                                    bool eligible) noexcept;

    [[nodiscard]] std::size_t
    formatDensityLine(char* buf, std::size_t cap,
                      float effectivePerChunk,
                      std::size_t configuredPerChunk,
                      std::int32_t chunkSizeWorld,
                      std::int32_t cellSizeWorld,
                      const sf::Vector2f& viewSize) noexcept;

    [[nodiscard]] std::size_t
    formatRangeLine(char* buf, std::size_t cap,
                    std::int32_t chunkMinX, std::int32_t chunkMinY,
                    std::int32_t chunkMaxX, std::int32_t chunkMaxY,
                    std::int32_t cellMinX, std::int32_t cellMinY,
                    std::int32_t cellMaxX, std::int32_t cellMaxY) noexcept;

    [[nodiscard]] std::size_t
    formatResidencyLine(char* buf, std::size_t cap,
                        const core::spatial::ChunkCoord& viewChunk,
                        core::spatial::ResidencyState viewState,
                        const core::spatial::ChunkCoord& playerChunk,
                        core::spatial::ResidencyState playerState,
                        const core::spatial::ChunkCoord& focusChunk,
                        core::spatial::ResidencyState focusState,
                        const core::spatial::ChunkCoord& originCurrent,
                        const core::spatial::ChunkCoord& originDesired,
                        std::uint32_t initialLoadRemaining,
                        std::uint32_t loadsThisFrame,
                        std::uint32_t unloadsThisFrame) noexcept;

#endif

    [[nodiscard]] inline std::size_t formatCameraStatsLine(char* buf, std::size_t cap,
                                                           float playerY, float viewCenterY,
                                                           float viewSizeY, float cameraOffsetY,
                                                           float cameraCenterYMax) noexcept {
        if (cap == 0) {
            return 0;
        }

        char* it = buf;
        char* end = buf + cap;

        if (!detail::appendLiteral(it, end, "Camera: playerY=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendFixed2(it, end, playerY)) {
            return static_cast<std::size_t>(it - buf);
        }

        if (!detail::appendLiteral(it, end, " centerY=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendFixed2(it, end, viewCenterY)) {
            return static_cast<std::size_t>(it - buf);
        }

        if (!detail::appendLiteral(it, end, " sizeY=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendFixed2(it, end, viewSizeY)) {
            return static_cast<std::size_t>(it - buf);
        }

        if (!detail::appendLiteral(it, end, " offsetY=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendFixed2(it, end, cameraOffsetY)) {
            return static_cast<std::size_t>(it - buf);
        }

        if (!detail::appendLiteral(it, end, " maxY=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendFixed2(it, end, cameraCenterYMax)) {
            return static_cast<std::size_t>(it - buf);
        }

        return static_cast<std::size_t>(it - buf);
    }

} // namespace game::skyguard::utils
