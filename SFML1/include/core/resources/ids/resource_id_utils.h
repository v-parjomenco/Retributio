// ================================================================================================
// File: core/resources/ids/resource_id_utils.h
// Purpose: String ↔ enum helpers for resource IDs (TextureID, FontID, SoundID)
// Used by: ResourcePaths, logging, debug tools
// Related headers: resource_ids.h, resource_paths.h
// ================================================================================================
#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "core/resources/ids/resource_ids.h"

namespace core::resources::ids {
    // --------------------------------------------------------------------------------------------
    // Универсальный хелпер для логов: idToString(T)
    // По умолчанию возвращает "Неизвестный ID", а для enum'ов использует toString(...) из
    // resource_ids.h. Для std::string возвращает саму строку.
    // --------------------------------------------------------------------------------------------
    template <typename Identifier> inline std::string idToString(const Identifier&) {
        return "Unknown ID";
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

    // --------------------------------------------------------------------------------------------
    // fromString(...) — парсинг строковых ID из JSON (resources.json), т.е. string -> enum
    // Возвращают std::optional<Enum>: nullopt, если имя не распознано.
    // --------------------------------------------------------------------------------------------
    inline std::optional<TextureID> textureFromString(std::string_view name) noexcept {
        if (name == "Player") {
            return TextureID::Player;
        }
        // TODO: extend as you add more TextureID values
        return std::nullopt;
    }

    inline std::optional<FontID> fontFromString(std::string_view name) noexcept {
        if (name == "Default") {
            return FontID::Default;
        }
        // TODO: extend as you add more FontID values
        return std::nullopt;
    }

    inline std::optional<SoundID> soundFromString(std::string_view name) noexcept {
        // Сейчас SoundID не расширен: мы намеренно НЕ распознаём ни одного имени.
        //
        // Контракт (чтобы не было WARN-спама):
        //  - пока здесь всегда nullopt, в resources.json не должно быть entries в блоке "sounds";
        //  - появятся звуки -> расширяем SoundID + добавляем маппинг здесь + регистрируем sounds в JSON.
        (void) name;
        return std::nullopt;
    }

} // namespace core::resources::ids