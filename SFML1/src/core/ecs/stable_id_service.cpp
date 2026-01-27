#include "pch.h"

#include "core/ecs/stable_id_service.h"

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

    StableIdService::StableId StableIdService::ensureAssigned(const Entity e) {
#if !defined(NDEBUG)
        debugAssertInvariants();
        assert(mEnabled && "StableIdService::ensureAssigned: service is disabled. Call enable().");
#else
        // В Release/Profile не делаем auto-enable: это программистская ошибка конфигурации.
        // Fail-fast: без StableID невозможно гарантировать корректность deterministic 
        // идентификаторов.
        if (!mEnabled) {
            LOG_PANIC(core::log::cat::ECS,
                      "StableIdService::ensureAssigned: service is disabled. "
                      "Call StableIdService::enable() before using Stable IDs.");
        }
#endif

        const std::size_t idx = rawIndex(e);
        ensureStorage(idx);

        const std::uint32_t verPlusOne = rawVersionPlusOne(e);

        const StableId existing = mStableIdByIndex[idx];
        if (existing != kUnsetStableId && mObservedVersionPlusOneByIndex[idx] == verPlusOne) {
            return existing;
        }

        const StableId newId = mNextStableId++;
        // 0 зарезервирован как sentinel и никогда не должен выдаваться.
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
        if (idx >= mStableIdByIndex.size()) {
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
        if (idx >= mStableIdByIndex.size()) {
            return;
        }

        // Version-aware invalidation:
        // защищает от recycled slot (тот же raw index, но другая версия).
        const std::uint32_t verPlusOne = rawVersionPlusOne(e);
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
        // ВАЖНО: mNextStableId НЕ сбрасываем (never reused).
        mStableIdByIndex.clear();
        mObservedVersionPlusOneByIndex.clear();
    }

    std::size_t StableIdService::rawIndex(const Entity e) noexcept {
        return static_cast<std::size_t>(entt::to_entity(e));
    }

    std::uint32_t StableIdService::rawVersionPlusOne(const Entity e) noexcept {
        const std::uint32_t ver = static_cast<std::uint32_t>(entt::to_version(e));

        // Железобетонная схема: 0 = unset, значит сохраняем version+1.
        // Переполнение ver+1 практически недостижимо в реальном EnTT usage;
        // в Debug ловим в assert.
        assert(ver != std::numeric_limits<std::uint32_t>::max() &&
               "StableIdService: entity version overflow (ver+1 would wrap).");

        return ver + 1u;
    }

    void StableIdService::ensureStorage(const std::size_t idx) {
#if !defined(NDEBUG)
        debugAssertInvariants();
#endif
        if (idx < mStableIdByIndex.size()) {
            return;
        }

        const std::size_t newSize = idx + 1u;
        mStableIdByIndex.resize(newSize, kUnsetStableId);
        mObservedVersionPlusOneByIndex.resize(newSize, kUnsetObservedVersionPlusOne);
    }

#if !defined(NDEBUG)
    void StableIdService::debugAssertInvariants() const {
        assert(mStableIdByIndex.size() == mObservedVersionPlusOneByIndex.size() &&
               "StableIdService: internal vectors desynchronized");
    }
#endif

} // namespace core::ecs
