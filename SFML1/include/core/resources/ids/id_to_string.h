#pragma once

#include <string>

#include "core/resources/ids/resourceIDs.h"

namespace core::resources::ids {
    // Универсальная функция — по умолчанию возвращает "Неизвестный ID"
    template <typename Identifier> inline std::string idToString(const Identifier&) {
        return u8"Неизвестный ID";
    }

    // Специализации для ожидаемых типов ID (enum'ы)
    template <> inline std::string idToString<TextureID>(const TextureID& id) {
        return std::string(toString(id));
    }

    template <> inline std::string idToString<FontID>(const FontID& id) {
        return std::string(toString(id));
    }

    template <> inline std::string idToString<SoundID>(const SoundID& id) {
        return std::string(toString(id));
    }

    // Специализация для std::string (динамические пути/ключи)
    template <> inline std::string idToString<std::string>(const std::string& id) {
        return id;
    }
} // namespace core::resources::ids