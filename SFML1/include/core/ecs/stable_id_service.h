// ================================================================================================
// File: core/ecs/stable_id_service.h
// Purpose: StableID core service (side-table) for deterministic ECS identifiers.
// Used by: core::ecs::World and systems requiring stable IDs.
// Related headers: core/ecs/entity.h
//
// Notes:
//  - StableID is monotonic uint64_t, never reused within a World session while enabled.
//  - Mapping is indexed by raw EnTT entity index (entt::to_entity(e)) and validated by generation
//    (version) to reject stale handles.
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

        // КОНТРАКТ НАЗНАЧЕНИЯ StableID (Titan-oriented):
        //
        // 1) ensureAssigned() — write-path:
        //    - вызывается ТОЛЬКО на write boundary (например, World::createEntity()).
        //    - запрещено вызывать из update/render систем и tight loops.
        //
        // 2) tryGet() — read-path:
        //    - используется в системах (включая deterministic сортировки).
        //    - никаких "late assignment" в update: если tryGet() вернул nullopt в режиме
        //      детерминизма — это ошибка wiring/контракта и должна фейлиться в месте вызова.
        //
        // 3) Zero allocations:
        //    - prewarm() вызывается в cold-path и фиксирует capacity внутренних таблиц.
        //    - после prewarm() ensureAssigned() НЕ имеет права расширять таблицы.
        [[nodiscard]] StableId ensureAssigned(Entity e);

        // Read-only lookup:
        //  - nullopt если сервис выключен,
        //  - nullopt если для этой сущности StableID ещё не назначен,
        //  - nullopt если handle устарел (generation mismatch).
        [[nodiscard]] std::optional<StableId> tryGet(Entity e) const;

        // Хук жизненного цикла: вызывать после registry.destroy() в flushDestroyed().
        // Инвалидация делается version-aware: stale handles не смогут "унаследовать" чужой ID.
        void onEntityDestroyed(Entity e) noexcept;

        // Сброс состояния сервиса для текущего мира:
        //  - очищает таблицы (без изменения capacity),
        //  - сбрасывает счётчик выдачи StableID на стартовое значение.
        void clear() noexcept;

        // Cold-path: фиксируем ёмкость таблиц (индексация по raw entity index).
        // Аргумент — это именно CAPACITY (max raw index + 1), а не aliveEntityCount().
        // После вызова таблицы считаются "замороженными": write-path не расширяет память.
        void prewarm(std::size_t capacity);

        [[nodiscard]] bool isPrewarmed() const noexcept;

      private:
        static constexpr StableId kUnsetStableId = 0u;

        // Стратегия observed version (железно):
        //  - observedVersionPlusOne == 0 => unset
        //  - observedVersionPlusOne == rawVersion(e) + 1 => match
        static constexpr std::uint32_t kUnsetObservedVersionPlusOne = 0u;

        [[nodiscard]] static std::size_t rawIndex(Entity e) noexcept;
        [[nodiscard]] static std::uint32_t rawVersionPlusOne(Entity e) noexcept;

#if !defined(NDEBUG)
        void debugAssertInvariants() const;
#endif

        bool mEnabled{false};

        // 0 зарезервирован как sentinel (kUnsetStableId), поэтому начинаем с 1.
        StableId mNextStableId{1u};

        // Таблицы фиксированной ёмкости после prewarm():
        // индексируются по entt::to_entity(e).
        std::vector<StableId> mStableIdByIndex{};
        std::vector<std::uint32_t> mObservedVersionPlusOneByIndex{};
    };

} // namespace core::ecs
