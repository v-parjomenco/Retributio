// ================================================================================================
// File: core/ui/ids/ui_id_utils.h
// Purpose: String <-> enum helpers for UI IDs
//          (AnchorType, ScalingBehaviorKind, LockBehaviorKind)
// Used by: Config loaders, debug tools, UI / ECS systems
// Related headers: core/ui/anchor_utils.h, core/ui/scaling_behavior.h,
//                  core/ui/lock_behavior.h
// ================================================================================================
#pragma once

#include <string_view>

#include "core/ui/anchor_utils.h"
#include "core/ui/lock_behavior.h"
#include "core/ui/scaling_behavior.h"

namespace core::ui::ids {

    // --------------------------------------------------------------------------------------------
    // enum -> string_view (основные имена для JSON / редакторов / логов)
    // --------------------------------------------------------------------------------------------

    std::string_view toString(AnchorType type) noexcept;
    std::string_view toString(ScalingBehaviorKind kind) noexcept;
    std::string_view toString(LockBehaviorKind kind) noexcept;

    // --------------------------------------------------------------------------------------------
    // string_view -> enum (парсинг JSON / пользовательского ввода)
    // --------------------------------------------------------------------------------------------

    [[nodiscard]] AnchorType
    anchorFromString(std::string_view name,
                     AnchorType defaultType = AnchorType::None) noexcept;

    [[nodiscard]] ScalingBehaviorKind
    scalingFromString(std::string_view name,
                      ScalingBehaviorKind defaultKind = ScalingBehaviorKind::None) noexcept;

    [[nodiscard]] LockBehaviorKind
    lockFromString(std::string_view name,
                   LockBehaviorKind defaultKind = LockBehaviorKind::World) noexcept;

    // --------------------------------------------------------------------------------------------
    // Унифицированный idToString для UI-идентификаторов (в духе resource_id_utils)
    // --------------------------------------------------------------------------------------------

    /**
     * @brief Универсальный хелпер для логов: idToString(T)
     *
     * По умолчанию возвращает "UnknownUiId", а для известных enum'ов использует
     * специализированные версии, завязанные на toString(...).
     */
    template <typename Identifier> inline std::string_view idToString(Identifier) noexcept {
        // Для неизвестных типов — просто метка.
        return u8"UnknownUiId";
    }

    // Специализации для AnchorType / ScalingBehaviorKind / LockBehaviorKind

    template <> inline std::string_view idToString<AnchorType>(AnchorType id) noexcept {
        return toString(id);
    }

    template <>
    inline std::string_view idToString<ScalingBehaviorKind>(ScalingBehaviorKind id) noexcept {
        return toString(id);
    }

    template <> inline std::string_view idToString<LockBehaviorKind>(LockBehaviorKind id) noexcept {
        return toString(id);
    }

} // namespace core::ui::ids