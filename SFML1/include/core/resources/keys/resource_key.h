// ================================================================================================
// File: core/resources/keys/resource_key.h
// Purpose: Runtime resource key (index + generation) for hot-path access.
// ================================================================================================
#pragma once

#include <cassert>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>

namespace core::resources {

    template <typename Tag> struct ResourceKey {
        static constexpr std::uint32_t InvalidRaw = 0u;

        static constexpr std::uint32_t IndexBits = 24u;
        static constexpr std::uint32_t GenerationShift = IndexBits;
        static constexpr std::uint32_t IndexMask = (1u << IndexBits) - 1u; // 0x00FFFFFF

        // Допустимый диапазон: 0..0x00FFFFFE.
        // 0x00FFFFFF запрещён, иначе (index+1) станет 0 и сломает кодировку.
        static constexpr std::uint32_t MaxIndex = IndexMask - 1u;

        std::uint32_t raw = InvalidRaw;

        constexpr ResourceKey() noexcept = default;

        [[nodiscard]] static constexpr ResourceKey make(std::uint32_t index,
                                                        std::uint8_t generation = 0u) noexcept {
#if !defined(NDEBUG)
            // validate-on-write: index обязан быть в контрактном диапазоне.
            assert(index <= MaxIndex);
#endif
            const std::uint32_t encodedIndex = index + 1u; // 1..IndexMask (валидно при контракте)
            const std::uint32_t encodedGeneration = static_cast<std::uint32_t>(generation)
                                                    << GenerationShift;

            return ResourceKey(encodedGeneration | encodedIndex);
        }

        [[nodiscard]] constexpr bool valid() const noexcept {
            return raw != InvalidRaw;
        }

        [[nodiscard]] constexpr std::uint32_t index() const noexcept {
#if !defined(NDEBUG)
            // trust-on-read: invalid отсекается на границах (load/resolve), не в hot path.
            assert(valid());
#endif
            return (raw & IndexMask) - 1u;
        }

        [[nodiscard]] constexpr std::uint8_t generation() const noexcept {
            return static_cast<std::uint8_t>(raw >> GenerationShift);
        }

        [[nodiscard]] constexpr std::uint32_t sortKey() const noexcept {
            return index();
        }

        auto operator<=>(const ResourceKey&) const = default;

      private:
        explicit constexpr ResourceKey(std::uint32_t rawValue) noexcept : raw(rawValue) {
        }
    };

    struct TextureTag {};
    struct FontTag {};
    struct SoundTag {};

    using TextureKey = ResourceKey<TextureTag>;
    using FontKey = ResourceKey<FontTag>;
    using SoundKey = ResourceKey<SoundTag>;

    static_assert(sizeof(TextureKey) == sizeof(std::uint32_t));
    static_assert(std::is_trivially_copyable_v<TextureKey>);
    static_assert(std::is_trivially_copyable_v<FontKey>);
    static_assert(std::is_trivially_copyable_v<SoundKey>);

    struct TextureKeyHash {
        [[nodiscard]] std::size_t operator()(TextureKey key) const noexcept {
            return std::hash<std::uint32_t>{}(key.raw);
        }
    };

    struct FontKeyHash {
        [[nodiscard]] std::size_t operator()(FontKey key) const noexcept {
            return std::hash<std::uint32_t>{}(key.raw);
        }
    };

    struct SoundKeyHash {
        [[nodiscard]] std::size_t operator()(SoundKey key) const noexcept {
            return std::hash<std::uint32_t>{}(key.raw);
        }
    };

} // namespace core::resources
