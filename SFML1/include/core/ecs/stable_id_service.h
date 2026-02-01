// ================================================================================================
// File: core/ecs/stable_id_service.h
// Purpose: StableID core service (side-table) for deterministic ECS identifiers.
// Used by: core::ecs::World and systems requiring stable IDs.
// Related headers: core/ecs/entity.h
//
// Notes:
//  - StableIDs are monotonic uint64_t, never reused.
//  - Lookup validates entity generation/version to avoid stale handles.
// ================================================================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "core/ecs/entity.h"

namespace core::ecs {

    class StableIdService {
      public:
        using StableId = std::uint64_t;

        [[nodiscard]] bool isEnabled() const noexcept;
        void enable() noexcept;

        // Детерминизм:
        //  - StableID детерминированен только при детерминированном порядке вызовов 
        //    ensureAssigned().
        //  - ensureAssigned() — write-path (может расширять таблицу); не вызывать в tight loops.
        [[nodiscard]] StableId ensureAssigned(Entity e);
        [[nodiscard]] std::optional<StableId> tryGet(Entity e) const;

        void onEntityDestroyed(Entity e) noexcept;
        void clear() noexcept;
        void prewarm(std::size_t maxEntities);

      private:
        static constexpr StableId kUnsetStableId = 0u;

        // Observed version storage strategy (ironclad):
        //  - observedVersionPlusOne == 0  => unset
        //  - observedVersionPlusOne == rawVersion(e) + 1 => match
        // Это исключает "магические" sentinel значения и не зависит от диапазона версии EnTT.
        static constexpr std::uint32_t kUnsetObservedVersionPlusOne = 0u;

        [[nodiscard]] static std::size_t rawIndex(Entity e) noexcept;
        [[nodiscard]] static std::uint32_t rawVersionPlusOne(Entity e) noexcept;

        void ensureStorage(std::size_t rawIndex);

#if !defined(NDEBUG)
        void debugAssertInvariants() const;
#endif

        bool mEnabled{false};

        // Начинаем с 1, потому что 0 = kUnsetStableId (sentinel, запрещённый StableID).
        // mNextStableId — "следующий выдаваемый" идентификатор.
        StableId mNextStableId{1u};

        std::vector<StableId> mStableIdByIndex{};
        std::vector<std::uint32_t> mObservedVersionPlusOneByIndex{};
    };

} // namespace core::ecs
