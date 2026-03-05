#include "pch.h"

#include "core/ecs/stable_id_service.h"

#include <algorithm>
#include <cassert>
#include <limits>

#include "adapters/entt/entt_registry.hpp"
#include "core/log/log_macros.h"

namespace core::ecs {

    bool StableIdService::isEnabled() const noexcept {
        return mEnabled;
    }

    void StableIdService::enable() noexcept {
        mEnabled = true;
    }

    bool StableIdService::isPrewarmed() const noexcept {
        return !mStableIdByIndex.empty();
    }

    StableIdService::StableId StableIdService::ensureAssigned(const Entity e) {
#if !defined(NDEBUG)
        debugAssertInvariants();
        assert(mEnabled && "StableIdService::ensureAssigned: service is disabled. Call enable().");
        assert(!mStableIdByIndex.empty() &&
               "StableIdService::ensureAssigned: service enabled but not prewarmed. "
               "Call prewarm(capacity) in cold-path before first update.");
#else
        if (!mEnabled) {
            LOG_PANIC(core::log::cat::ECS,
                      "StableIdService::ensureAssigned: service is disabled. Call enable().");
        }
        if (mStableIdByIndex.empty()) {
            LOG_PANIC(core::log::cat::ECS,
                      "StableIdService::ensureAssigned: service enabled but not prewarmed. "
                      "Call prewarm(capacity) in cold-path before first update.");
        }
#endif

        const std::size_t idx = rawIndex(e);

        if (idx >= mStableIdByIndex.size()) [[unlikely]] {
            LOG_PANIC(core::log::cat::ECS,
                      "StableIdService::ensureAssigned: raw index {} >= capacity {}. "
                      "Increase prewarm(capacity).",
                      idx, mStableIdByIndex.size());
        }

        const std::uint32_t verPlusOne = rawVersionPlusOne(e);

        const StableId existing = mStableIdByIndex[idx];
        if (existing != kUnsetStableId && mObservedVersionPlusOneByIndex[idx] == verPlusOne) {
            return existing;
        }

        const StableId newId = mNextStableId++;
        assert(newId != kUnsetStableId && "StableIdService: StableID overflow (wrapped to 0).");

        mStableIdByIndex[idx] = newId;
        mObservedVersionPlusOneByIndex[idx] = verPlusOne;
        return newId;
    }

    std::optional<StableIdService::StableId> StableIdService::tryGet(const Entity e) const {
#if !defined(NDEBUG)
        debugAssertInvariants();
#endif
        if (!mEnabled) {
            return std::nullopt;
        }

        const std::size_t idx = rawIndex(e);

        if (idx >= mStableIdByIndex.size()) [[unlikely]] {
#if !defined(NDEBUG)
            assert(false && "StableIdService::tryGet: entity raw index exceeds prewarmed capacity. "
                            "Either prewarm() not called or capacity too small.");
#endif
            return std::nullopt;
        }

        const StableId existing = mStableIdByIndex[idx];
        if (existing == kUnsetStableId) {
            return std::nullopt;
        }

        const std::uint32_t verPlusOne = rawVersionPlusOne(e);
        if (mObservedVersionPlusOneByIndex[idx] != verPlusOne) {
            return std::nullopt;
        }

        return existing;
    }

    void StableIdService::onEntityDestroyed(const Entity e) noexcept {
#if !defined(NDEBUG)
        debugAssertInvariants();
#endif
        if (!mEnabled) {
            return;
        }

        const std::size_t idx = rawIndex(e);

        if (idx >= mStableIdByIndex.size()) [[unlikely]] {
            LOG_PANIC(core::log::cat::ECS,
                      "StableIdService::onEntityDestroyed: raw index {} >= capacity {}. "
                      "Increase prewarm(capacity).",
                      idx, mStableIdByIndex.size());
        }

        const std::uint32_t verPlusOne = rawVersionPlusOne(e);

        // Version-aware invalidation: защищает от recycled slot (тот же raw index, другая версия).
        if (mObservedVersionPlusOneByIndex[idx] != verPlusOne) {
            return;
        }

        mStableIdByIndex[idx] = kUnsetStableId;
        mObservedVersionPlusOneByIndex[idx] = kUnsetObservedVersionPlusOne;
    }

    void StableIdService::clear() noexcept {
#if !defined(NDEBUG)
        debugAssertInvariants();
#endif
        if (!mEnabled) {
            // Если сервис выключен — состояния как такового нет.
            // Ничего не делаем: нулевая стоимость.
            return;
        }

        // ВАЖНО: capacity сохраняем (никаких dealloc), чистим содержимое.
        std::fill(mStableIdByIndex.begin(), mStableIdByIndex.end(), kUnsetStableId);
        std::fill(mObservedVersionPlusOneByIndex.begin(), mObservedVersionPlusOneByIndex.end(),
                  kUnsetObservedVersionPlusOne);

        // Новая "сессия" StableID внутри мира (World::clear()).
        mNextStableId = 1u;
    }

    void StableIdService::prewarm(const std::size_t capacity) {
#if !defined(NDEBUG)
        debugAssertInvariants();
        assert(mEnabled && "StableIdService::prewarm: service is disabled. Call enable().");
#else
        if (!mEnabled) {
            LOG_PANIC(core::log::cat::ECS,
                      "StableIdService::prewarm: service is disabled. Call enable().");
        }
#endif

        if (capacity == 0u) {
            return;
        }

        if (mStableIdByIndex.size() >= capacity) {
            return;
        }

        // Единственная точка потенциальных аллокаций (cold-path).
        mStableIdByIndex.resize(capacity, kUnsetStableId);
        mObservedVersionPlusOneByIndex.resize(capacity, kUnsetObservedVersionPlusOne);
    }

    std::size_t StableIdService::rawIndex(const Entity e) noexcept {
        return static_cast<std::size_t>(entt::to_entity(e));
    }

    std::uint32_t StableIdService::rawVersionPlusOne(const Entity e) noexcept {
        const std::uint32_t ver = static_cast<std::uint32_t>(entt::to_version(e));

        assert(ver != std::numeric_limits<std::uint32_t>::max() &&
               "StableIdService: entity version overflow (ver+1 would wrap).");

        return ver + 1u;
    }

#if !defined(NDEBUG)
    void StableIdService::debugAssertInvariants() const {
        assert(mStableIdByIndex.size() == mObservedVersionPlusOneByIndex.size() &&
               "StableIdService: internal vectors desynchronized");
    }
#endif

} // namespace core::ecs
