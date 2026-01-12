// ================================================================================================
// File: core/resources/ids/resource_id_utils.h
// Purpose: String ↔ enum helpers for resource IDs (TextureID, FontID, SoundID)
// Used by: ResourcePaths, logging, debug tools
// Related headers: resource_ids.h
//
// Notes:
//  - COLD PATH ONLY: эти функции предназначены для логов/диагностики/ошибок.
//    Запрещено вызывать их в tight-loop / hot path (RenderSystem per-entity и т.п.).
// ================================================================================================
#pragma once

#include <string>
#include <string_view>

#include "core/resources/ids/resource_ids.h"

namespace core::resources::ids {

    // --------------------------------------------------------------------------------------------
    // Универсальный хелпер для логов: idToString(T)
    //
    // Контракт:
    //  - ResourceId enum'ы -> string_view без аллокаций.
    //  - std::string -> string_view (без копирования).
    //
    // Примечание о времени жизни string_view:
    //  - Возвращаемый std::string_view обязан жить дольше, чем выполняется вызов логгера.
    //    В нашем логгере форматирование выполняется синхронно, поэтому это безопасно.
    // --------------------------------------------------------------------------------------------

    template <ResourceId Identifier>
    [[nodiscard]] constexpr std::string_view idToString(Identifier id) noexcept {
        return toString(id);
    }

    [[nodiscard]] inline std::string_view idToString(const std::string& id) noexcept {
        return id;
    }

} // namespace core::resources::ids
