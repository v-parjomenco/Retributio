// ================================================================================================
// File: core/spatial/chunk_coord.h
// Purpose: Unified chunk coordinate contract (tile-agnostic)
// Used by: SpatialIndex v2, streaming, AI/visibility (future)
// Related headers: (none)
// Notes: Tile-agnostic POD; conversion helpers are constexpr-friendly where possible.
// ================================================================================================
#pragma once

#include <cmath>
#include <cstdint>
#include <type_traits>

namespace core::spatial {

    struct ChunkCoord final {
        std::int32_t x = 0;
        std::int32_t y = 0;

        [[nodiscard]] constexpr bool operator==(const ChunkCoord&) const noexcept = default;
    };

    template <typename T>
    struct WorldPosT final {
        T x{};
        T y{};
    };

    template <typename T>
    struct ChunkRectT final {
        T minX{};
        T minY{};
        T maxX{};
        T maxY{};
    };

    using WorldPosf = WorldPosT<float>;
    using WorldPosi = WorldPosT<std::int32_t>;
    using ChunkRectf = ChunkRectT<float>;
    using ChunkRecti = ChunkRectT<std::int32_t>;

    namespace detail {
        [[nodiscard]] inline std::int32_t floorDivInt(const std::int32_t v,
                                                      const std::int32_t d) noexcept {
            const std::int32_t q = v / d;
            const std::int32_t r = v % d;
            if (r != 0 && ((r > 0) != (d > 0))) {
                return q - 1;
            }
            return q;
        }

        template <typename T>
        [[nodiscard]] inline std::int32_t floorDiv(const T v, const std::int32_t d) noexcept {
            if constexpr (std::is_floating_point_v<T>) {
                return static_cast<std::int32_t>(
                    std::floor(static_cast<double>(v) / static_cast<double>(d)));
            } else {
                return floorDivInt(static_cast<std::int32_t>(v), d);
            }
        }
    } // namespace detail

    template <typename T>
    [[nodiscard]] inline ChunkCoord worldToChunk(const WorldPosT<T> p,
                                                 const std::int32_t chunkSizeWorld) noexcept {
        return ChunkCoord{detail::floorDiv(p.x, chunkSizeWorld),
                          detail::floorDiv(p.y, chunkSizeWorld)};
    }

    template <typename T>
    [[nodiscard]] inline WorldPosT<T> chunkOrigin(const ChunkCoord c,
                                                  const std::int32_t chunkSizeWorld) noexcept {
        return WorldPosT<T>{static_cast<T>(c.x) * static_cast<T>(chunkSizeWorld),
                            static_cast<T>(c.y) * static_cast<T>(chunkSizeWorld)};
    }

    template <typename T>
    [[nodiscard]] inline ChunkRectT<T> chunkBounds(const ChunkCoord c,
                                                   const std::int32_t chunkSizeWorld) noexcept {
        const WorldPosT<T> origin = chunkOrigin<T>(c, chunkSizeWorld);
        const T size = static_cast<T>(chunkSizeWorld);
        return ChunkRectT<T>{origin.x, origin.y, origin.x + size, origin.y + size};
    }

} // namespace core::spatial