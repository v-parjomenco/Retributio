#pragma once

#include <cstdint>

namespace core::resources::ids {

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

    // Утилиты для логирования/ошибок — inline функции, возвращающие читаемую строку
    // (не меняют API, не добавляют зависимостей, бесплатная помощь при дебаге)
    inline const char* toString(TextureID id) noexcept {
        switch (id) {
        case TextureID::Player: return "Player";
        case TextureID::Unknown: return "Unknown";
        default: return "Unknown";
        }
    }

    inline const char* toString(FontID id) noexcept {
        switch (id) {
        case FontID::Default: return "Default";
        case FontID::Unknown: return "Unknown";
        default: return "Unknown";
        }
    }

    inline const char* toString(SoundID id) noexcept {
        switch (id) {
        case SoundID::Unknown: return "Unknown";
        default: return "Unknown";
        }
    }

} // namespace core::resources::ids