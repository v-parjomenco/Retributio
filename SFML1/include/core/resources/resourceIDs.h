#pragma once

#include <cstdint>

namespace core::resources {

    // Текстуры
    enum class TextureID : uint8_t {
        Unknown = 0,
        Player
    };

    // Шрифты
    enum class FontID : uint8_t {
        Unknown = 0,
        Default
    };

    // Звуки
    enum class SoundID : uint8_t {
        Unknown = 0
    };

} // namespace core::resources