#include "pch.h"

#include "core/ecs/world.h"

#include <cstdint>
#include <exception>
#include <limits>

#include "core/log/log_macros.h"

namespace core::ecs {

    World::World() noexcept
        : World(CreateInfo{}) {
    }

    World::World(const CreateInfo& info) noexcept
        : mRegistry() {
        // КРИТИЧНО: reserve() ПЕРЕД любыми component storage.reserve() и create().
        // Это гарантирует, что entity pool выделяется первым и избегает realloc
        // при добавлении entities.
        if (info.reserveEntities != 0) {
            prewarmRegistry_(info.reserveEntities);
        }

        // ВАЖНО: любые reserve() для component storage<T> должны идти ПОСЛЕ prewarmRegistry_().
        // Это обеспечивает правильный порядок инициализации и избегает heap churn.
        //
        // Пример (если когда-нибудь понадобится):
        // mRegistry.storage<SomeComponent>().reserve(expectedComponentCount);
    }

    void World::prewarmRegistry_(const std::size_t reserveEntities) noexcept {
        // ----------------------------------------------------------------------------------------
        // FAIL-FAST: EnTT HARD LIMIT (зависит от ENTT_ID_TYPE)
        // ----------------------------------------------------------------------------------------
        //
        // КОНТРАКТ:
        //  - entity_traits<T>::entity_mask определяет максимальный индекс сущности
        //  - Дефолт (uint32): entity_mask = 0xFFFFF (20 бит) → 1,048,576 max
        //  - С uint64: entity_mask = 0xFFFFFFFF (32 бита) → 4,294,967,296 max
        //
        // КРИТИЧНО:
        //  - Превышение лимита = undefined behavior (wraparound, collision, crash)
        //  - Fail-fast в конструкторе > загадочный краш через час игры
        const std::uint64_t maxEntities =
            static_cast<std::uint64_t>(entt::entt_traits<entt::entity>::entity_mask) + 1ull;

        if (static_cast<std::uint64_t>(reserveEntities) > maxEntities) {
            LOG_PANIC(core::log::cat::ECS,
                      "World: requested reserveEntities={} exceeds EnTT hard limit={} "
                      "(entity_mask=0x{:X}). Check ENTT_ID_TYPE (current={} bytes).",
                      reserveEntities, maxEntities,
                      entt::entt_traits<entt::entity>::entity_mask,
                      sizeof(entt::entity));
        }

        // ----------------------------------------------------------------------------------------
        // RESERVE: ZERO-ALLOCATION HOT-PATH
        // ----------------------------------------------------------------------------------------
        //
        // ПОЧЕМУ ЭТО КРИТИЧНО:
        //  - Без reserve: EnTT растёт динамически (каждые ~10 create → realloc)
        //  - Realloc = heap lock + memcpy + cache miss + потенциальный page fault
        //  - 1.6M entities без reserve = ~160 realloc за игровой цикл
        //  - 1.6M entities с reserve(1.6M) = 0 realloc (вся память из prewarm)
        //
        // AAA STANDARD:
        //  - Unity: EntityManager.SetEntityCapacity()
        //  - Unreal: World.PreAllocateEntityArray()
        //  - Paradox (Stellaris): MemoryConfig.xml с pre-allocation
        //
        // КРИТИЧНО ДЛЯ DETERMINISM:
        //  - Без reserve: порядок аллокаций зависит от heap state (недетерминированно)
        //  - С reserve: весь heap выделен заранее (детерминированно)
        //
        try {
            mRegistry.storage<entt::entity>().reserve(reserveEntities);
        } catch (const std::bad_alloc&) {
            // Невозможно продолжить без памяти: лучше упасть чистым PANIC,
            // чем получить heap corruption или weird crashes позже.
            LOG_PANIC(core::log::cat::ECS,
                      "World: EnTT registry.reserve({}) failed (bad_alloc). "
                      "Required memory: ~{} MB (entity pool only). "
                      "Check system RAM / virtual address space.",
                      reserveEntities,
                      (reserveEntities * sizeof(entt::entity)) / (1024 * 1024));
        } catch (...) {
            // Экзотические exception (например, platform-specific allocation errors).
            // Теоретически невозможно, но fail-fast всё равно лучше, чем UB.
            LOG_PANIC(core::log::cat::ECS,
                      "World: EnTT registry.reserve({}) failed (unknown exception). "
                      "This should never happen. Report to engine team.",
                      reserveEntities);
        }

        // ----------------------------------------------------------------------------------------
        // ДИАГНОСТИКА (логируем успешный prewarm)
        // ----------------------------------------------------------------------------------------
        //
        // ЗАЧЕМ:
        //  - Верификация, что FIX применился (видно в логе при старте)
        //  - Детектирует "забыли передать CreateInfo" (тогда capacity будет 0)
        //  - Профилирование memory footprint (entity pool size в MB)
        //
        // КРИТИЧНО ДЛЯ DEBUG:
        //  - Без этого лога невозможно понять, почему краш при 1M+ entities
        //  - С логом: сразу видно "reserved 1,615,186" или "reserved 0" (forgot CreateInfo)
        LOG_INFO(core::log::cat::ECS,
                 "World: EnTT entity pool reserved (capacity={}, bytes={}, entity_id_size={})",
                 reserveEntities,
                 reserveEntities * sizeof(entt::entity),
                 sizeof(entt::entity));
    }

} // namespace core::ecs