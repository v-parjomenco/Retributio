// ================================================================================================
// File: core/ecs/world.h
// Purpose: Zero-overhead ECS façade over EnTT (entt::registry) + SystemManager.
// Used by: Game layer, all ECS systems.
// Related headers: core/ecs/entity.h, core/ecs/system_manager.h
//
// Notes:
//  - Single-threaded for now. No method here is thread-safe by itself.
//  - group() is intentionally NON-const: EnTT may need to mutate internal state to form a group.
//    Read-only code should use view() const.
// ================================================================================================
#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

#include "adapters/entt/entt_registry.hpp"

#include "core/ecs/entity.h"
#include "core/ecs/stable_id_service.h"
#include "core/ecs/system_manager.h"

namespace sf {
    class RenderWindow;
} // namespace sf

namespace core::ecs {

    /**
     * @brief Политика допустимого ECS-компонента (фильтр на этапе компиляции).
     *
     * Требования проекта:
     *  - POD-подобный: trivially copyable (SoA-friendly, быстрая сериализация);
     *  - "Голый" тип: без const/volatile и без ссылок (EnTT хранит не-cvref типы);
     *  - Без владения ресурсами: trivially destructible (никаких нетривиальных деструкторов).
     *
     * Примечание по строгости дизайна:
     *  - Этот концепт намеренно СТРОГИЙ для core ECS компонентов.
     *  - Для save/load могут потребоваться relaxed компоненты с кастомным конструктором.
     *  - Такие случаи должны использовать отдельный концепт SaveComponent.
     */
    template <typename T>
    concept Component = std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T> &&
                        !std::is_const_v<T> && !std::is_volatile_v<T> && !std::is_reference_v<T>;

    /**
     * @brief Политика data-компонента (не пустой).
     *
     * Data-компоненты несут данные и должны добавляться через addComponent<T>().
     * Tag-компоненты (empty) добавляются только через addTag<T>().
     */
    template <typename T>
    concept DataComponent = Component<T> && !std::is_empty_v<T>;

    /**
     * @brief Политика tag-компонента (пустой маркер для отслеживания dirty).
     *
     * Примечание по дизайну:
     *  - std::is_empty_v<T> недостаточно строго: любой пустой struct пройдёт.
     *  - TagComponent явно указывает семантику: это маркер, не data.
     *  - Используется для ограничения World::markDirty<T> (защита от неправильного использования).
     *
     * Требования:
     *  - Базовый компонент (trivially copyable и т.п.)
     *  - Пустой тип (без полей данных)
     *
     * Примеры:
     *  ✅ struct SpatialDirtyTag {};
     *  ❌ struct Health { int value; };   // Не пустой
     *  ❌ struct NetworkDirty {};         // Пустой, но не задуман как tag (нужен явный opt-in)
     */
    template <typename T>
    concept TagComponent = Component<T> && std::is_empty_v<T>;

    /**
     * @brief Главный координатор ECS (тонкий фасад над entt::registry).
     *
     * Цели дизайна:
     *  - Zero overhead: всё инлайнится и сводится к прямым вызовам EnTT;
     *  - Fail-fast в Debug: assert на инварианты, нулевая стоимость в Release;
     *  - Жёсткая политика компонентов: концепт Component режет неподходящие типы на компиляции.
     *
     * КРИТИЧНЫЕ ИНВАРИАНТЫ (жизненный цикл сущностей):
     *  1. createEntity() — единственный способ создания сущностей
     *  2. destroyEntity() — единственный способ планирования уничтожения сущностей
     *  3. flushDestroyed() — единственная точка фактического уничтожения
     *  4. mAliveEntityCount синхронизирован с registry (Debug: validateEntityCount)
     *  5. registry() доступен ТОЛЬКО через friend (контролируемый escape hatch)
     *  6. Friend-классы НЕ должны вызывать registry.create/destroy напрямую
     */
    class World {
      public:
        /**
         * @brief Параметры создания World (capacity prewarm, feature toggles).
         *
         * АРХИТЕКТУРНОЕ РЕШЕНИЕ:
         *  - World владеет registry → World отвечает за его capacity policy
         *  - Game остаётся дирижёром: передаёт expectedMaxEntities, не лезет в registry
         *  - Единая точка истины для capacity (одна write-boundary, детерминизм)
         *
         * ЗАЧЕМ НУЖЕН reserveEntities:
         *  - EnTT создаёт entities через free-list с динамическим ростом
         *  - Без prewarm: каждая 10-я create() вызывает realloc (cache miss, heap churn)
         *  - С prewarm: вся память выделяется заранее (zero allocations в hot-path)
         *
         * КРИТИЧНО ДЛЯ ПРОИЗВОДИТЕЛЬНОСТИ:
         *  - Без reserve(): ~1.6M entities = ~160 heap allocations за игровой цикл
         *  - С reserve(1.6M): ~1.6M entities = 0 heap allocations (вся память из prewarm)
         *  - Стандарт Unity/Unreal: все пулы резервируются при загрузке уровня
         */
        struct CreateInfo final {
            /**
             * @brief Ожидаемое максимальное количество живых entities (для prewarm).
             *
             * КОНТРАКТ:
             *  - 0 = без prewarm (дефолтное поведение EnTT, динамический рост)
             *  - >0 = prewarm до указанной capacity (fail-fast если превышает лимит EnTT)
             *
             * ОТКУДА БРАТЬ ЗНАЧЕНИЕ:
             *  - Atrapacielos: SpatialIndexSystemConfig::maxEntityId (включает headroom)
             *  - Общий случай: max(streaming entities, non-streaming entities) × safety margin
             *
             * ВАЖНО:
             *  - Это НЕ hard limit (EnTT может расти дальше при необходимости)
             *  - Это capacity hint для избежания realloc в типичных сценариях
             */
            std::size_t reserveEntities = 0;
        };

        /**
         * @brief Дефолтный конструктор (без prewarm, динамический рост).
         *
         * Эквивалентно World(CreateInfo{}).
         */
        World() noexcept;

        /**
         * @brief Конструктор с параметрами (capacity prewarm, feature toggles).
         *
         * КРИТИЧНО:
         *  - reserve() выполняется ДО любых registry.create()
         *  - reserve() выполняется ДО любых storage<T>.reserve()
         *  - Fail-fast при превышении EnTT limit или bad_alloc
         *
         * @param info Параметры создания (reserveEntities и т.п.)
         */
        explicit World(const CreateInfo& info) noexcept;

        ~World() = default;

        // World — единственный владелец реестра, копирование запрещаем.
        World(const World&) = delete;
        World& operator=(const World&) = delete;

        // ----------------------------------------------------------------------------------------
        // Семантика перемещения (КРИТИЧНО: требуется ручная реализация)
        // ----------------------------------------------------------------------------------------

        /**
         * @brief Move-конструктор (ручная реализация для корректного переноса счётчика).
         *
         * CRITICAL BUG FIX:
         *  - Default move копирует примитивные типы (mAliveEntityCount);
         *  - После std::move(w1) старый объект w1 остаётся с mAliveEntityCount > 0;
         *  - Но registry пуст → рассинхронизация!
         *
         * РЕШЕНИЕ:
         *  - Ручная реализация с обнулением счётчика у источника;
         *  - Источник после move остаётся в валидном moved-from состоянии (count=0).
         *
         * ИНВАРИАНТ:
         *  - После move: source.aliveEntityCount() == 0;
         *  - После move: dest.aliveEntityCount() == old(source.aliveEntityCount()).
         *
         * @param other Источник перемещения (будет обнулён)
         */
        World(World&& other) noexcept
            : mRegistry(std::move(other.mRegistry)), mSystems(std::move(other.mSystems)),
              mStableIds(std::move(other.mStableIds)),
              mAliveEntityCount(other.mAliveEntityCount),
              mDeferredDestroyQueue(std::move(other.mDeferredDestroyQueue))
#if !defined(NDEBUG)
            , mQueuedDestroyStampByIndex(std::move(other.mQueuedDestroyStampByIndex))
            , mQueuedDestroyStamp(other.mQueuedDestroyStamp)
            , mDebugUpdateInProgress(other.mDebugUpdateInProgress)
            , mDebugFlushInProgress(other.mDebugFlushInProgress)
            , mRequireStableIdsForDeterminism(other.mRequireStableIdsForDeterminism)
#endif
        {
            other.mAliveEntityCount = 0; // КРИТИЧНО: обнуляем источник
#if !defined(NDEBUG)
            // moved-from мир не должен "залипать" в фазах.
            other.mDebugUpdateInProgress = false;
            other.mDebugFlushInProgress = false;
            other.mRequireStableIdsForDeterminism = false;
#endif
        }

        /**
         * @brief Move-оператор присваивания (ручная реализация для корректного переноса счётчика).
         *
         * CRITICAL BUG FIX:
         *  - Default move-assign копирует примитивы → рассинхронизация.
         *
         * РЕШЕНИЕ:
         *  - Ручная реализация с обнулением счётчика у источника;
         *  - Self-assignment защита (if this != &other).
         *
         * ИНВАРИАНТ:
         *  - После move: source.aliveEntityCount() == 0;
         *  - После move: this->aliveEntityCount() == old(source.aliveEntityCount()).
         *
         * @param other Источник перемещения (будет обнулён)
         * @return *this
         */
        World& operator=(World&& other) noexcept {
            if (this != &other) {
                mRegistry = std::move(other.mRegistry);
                mSystems = std::move(other.mSystems);
                mStableIds = std::move(other.mStableIds);
                mAliveEntityCount = other.mAliveEntityCount;
                mDeferredDestroyQueue = std::move(other.mDeferredDestroyQueue);
#if !defined(NDEBUG)
                mQueuedDestroyStampByIndex = std::move(other.mQueuedDestroyStampByIndex);
                mQueuedDestroyStamp = other.mQueuedDestroyStamp;
                mDebugUpdateInProgress = other.mDebugUpdateInProgress;
                mDebugFlushInProgress = other.mDebugFlushInProgress;
                mRequireStableIdsForDeterminism = other.mRequireStableIdsForDeterminism;

                other.mDebugUpdateInProgress = false;
                other.mDebugFlushInProgress = false;
                other.mRequireStableIdsForDeterminism = false;
#endif
                other.mAliveEntityCount = 0; // КРИТИЧНО: обнуляем источник
            }
            return *this;
        }

        // ----------------------------------------------------------------------------------------
        // Жизненный цикл сущностей
        // ----------------------------------------------------------------------------------------

        /**
         * @brief Создать новую сущность.
         *
         * ИНВАРИАНТ: Increment mAliveEntityCount (для O(1) диагностики).
         *
         * Сложность: O(1)
         * Потокобезопасность: Небезопасно.
         */
        [[nodiscard]] Entity createEntity() {
#if !defined(NDEBUG)
            if (mRequireStableIdsForDeterminism) {
                const auto& ids = stableIds();
                assert(ids.isEnabled() && ids.isPrewarmed() &&
                       "World::createEntity: deterministic mode requires StableIdService "
                       "enabled+prewarmed BEFORE any entity creation. "
                       "Fix Game wiring (enable+prewarm in init).");
            }
#endif

            const Entity e = mRegistry.create();
            ++mAliveEntityCount;

            // Контракт: если StableIdService включён, StableID назначается на write-boundary.
            if (mStableIds.isEnabled()) {
                (void) mStableIds.ensureAssigned(e);
            }

            return e;
        }

        /**
         * @brief Prewarm/maintenance: зарезервировать ёмкость очереди deferred destroy.
         *
         * Зачем это нужно:
         *  - "no heap churn per destroy" достигается не микро-логикой роста на каждом destroy,
         *    а контролируемой аллокацией в cold-пути (init/maintenance/перед стрессом).
         *
         * Контракт:
         *  - Вызывать в cold-пути (например, после initWorld или перед крупной сценой/стрессом).
         *  - В hot-пути destroyDeferred()/destroyEntities() не должно быть скрытых reserve().
         */
        void reserveDeferredDestroyQueue(std::size_t capacity) {
            mDeferredDestroyQueue.reserve(capacity);
        }

        /**
         * @brief Планировать уничтожение сущности (deferred destroy).
         *
         * Контракт:
         *  - Фактическое уничтожение происходит ТОЛЬКО в flushDestroyed()
         *  - on_destroy hooks вызываются во время flushDestroyed()
         *  - В течение текущей фазы сущность считается живой
         *
         * ВАЖНО (контракт):
         *  - destroyDeferred()/destroyEntity() НЕ удаляют сущность "мгновенно".
         *  - Если нужно немедленно исключить сущность из логики/рендера в этом же тике,
         *    используй отдельный state/tag (например, Disabled/Hidden/Inactive), а не destroy.
         *  - Любые системы НЕ должны полагаться на "мгновенное исчезновение" после destroyEntity().
         *
         * Сложность: O(1) amortized.
         * Потокобезопасность: Небезопасно.
         */
        void destroyDeferred(Entity e) {
#if !defined(NDEBUG)
            assert(!mDebugFlushInProgress &&
                   "destroyDeferred: cannot enqueue entities while flushDestroyed() is running");
#endif
            assert(isAlive(e) && "destroyDeferred: entity must be alive");

#if !defined(NDEBUG)
            debugMarkQueuedForDestroy(e);
#endif
            mDeferredDestroyQueue.push_back(e);
        }

        /**
         * @brief Уничтожить сущность (deferred).
         *
         * ВАЖНО: уничтожение происходит только в flushDestroyed().
         * Это alias для destroyDeferred() ради обратной совместимости.
         *
         * ВАЖНО (контракт):
         *  - destroyEntity() НЕ удаляет сущность "мгновенно" (она остаётся alive до flushDestroyed()).
         *  - Для "сразу выключить" используй tag/state, а не destroy.
         */
        void destroyEntity(Entity e) {
            destroyDeferred(e);
        }

        /**
         * @brief Планировать уничтожение нескольких сущностей (batch enqueue).
         *
         * КРИТИЧНО ДЛЯ ПРОИЗВОДИТЕЛЬНОСТИ:
         *  - Для 500k+ entities удаление 50k сущностей в цикле destroyEntity() медленно
         *  - Batch enqueue избегает лишних проверок и аллокаций
         *  - Типичный сценарий использования: удаление всех юнитов фракции после поражения
         *
         * ИНВАРИАНТ: Decrement mAliveEntityCount происходит в flushDestroyed().
         *
         * ТРЕБОВАНИЯ:
         *  - Все сущности в диапазоне должны быть валидны (isAlive)
         *  - Observers on_destroy получат события для всех сущностей во время flushDestroyed()
         *
         * @warning НЕ передавайте итераторы напрямую из registry.view() или registry.each()!
         *          EnTT может инвалидировать их во время удаления.
         *          Всегда копируйте в std::vector<Entity> перед вызовом.
         *
         * @pre [first, last) должен быть СТАБИЛЬНЫМ диапазоном (std::vector, std::array, std::span).
         *      Передача итераторов EnTT view = потенциальный UB.
         * @pre ВСЕ сущности в [first, last) ОБЯЗАНЫ быть живыми (isAlive() == true).
         *      Передача destroyed/recycled entities = UB в Release, assert в Debug.
         *      Вызывающая сторона ответственна за то, чтобы не хранить «устаревшие» ID сущностей
         *      после уничтожения.
         * @pre [first, last) ОБЯЗАНЫ содержать уникальные сущности (не дубликаты).
         *      Дубликаты нарушают контракт и синхронизацию mAliveEntityCount.
         *
         * @tparam Iterator RandomAccessIterator над Entity
         * @param first Начало диапазона
         * @param last Конец диапазона (exclusive)
         *
         * Сложность: O(N)
         * Потокобезопасность: Небезопасно.
         */
        template <std::random_access_iterator Iterator>
            requires std::same_as<std::iter_value_t<Iterator>, Entity>
        void destroyEntities(Iterator first, Iterator last) {
#if !defined(NDEBUG)
            assert(!mDebugFlushInProgress &&
                   "destroyEntities: cannot enqueue entities while flushDestroyed() is running");
#endif
            // O(1) distance для random_access_iterator (гарантировано концептом)
            const auto signedDist = last - first;
            assert(signedDist >= 0 && "destroyEntities: invalid iterator range (last < first)");

            const std::size_t count = static_cast<std::size_t>(signedDist);

            assert(count <= mAliveEntityCount && "destroyEntities: count exceeds alive entities "
                                                 "(invalid iterators or double-destroy)");

#if !defined(NDEBUG)
            // Debug: validate-on-write без O(N×M). Дубликаты и повторное enqueue детектим по stamp-маске.
            for (auto it = first; it != last; ++it) {
                const Entity e = *it;
                assert(isAlive(e) && "destroyEntities: iterator contains invalid entity");
                debugMarkQueuedForDestroy(e);
            }
#endif

            mDeferredDestroyQueue.insert(mDeferredDestroyQueue.end(), first, last);
        }

        /**
         * @brief Уничтожить несколько сущностей из контейнера (удобная перегрузка).
         *
         * КРИТИЧНО ДЛЯ ПРОИЗВОДИТЕЛЬНОСТИ:
         *  - std::span/vector → batch destroy
         *  - Contiguous memory = cache-friendly iteration
         *
         * ОГРАНИЧЕНИЕ (на этапе компиляции):
         *  - Требует contiguous_range (vector, array, span)
         *  - Требует sized_range (O(1) size())
         *  - Требует common_range (begin/end одного типа)
         *  - std::list, lazy ranges и т.п. НЕ компилируются
         *  - Это защита от случайного использования cache-unfriendly контейнеров
         *
         * @tparam Container Контейнер с Entity (vector, array, span, C-array)
         * @param entities Контейнер сущностей для удаления
         *
         * Пример:
         *  std::vector<Entity> defeated = {e1, e2, e3};
         *  world.destroyEntities(defeated);
         *
         *  // Или через span:
         *  std::span<const Entity> toDestroy = ...;
         *  world.destroyEntities(toDestroy);
         *
         * Сложность: O(N × M), где N — количество сущностей, M — средние компоненты.
         * Потокобезопасность: Небезопасно.
         */
        template <typename Container>
            requires std::ranges::contiguous_range<Container> && std::ranges::sized_range<Container>
                     && std::ranges::common_range<Container>
                     && std::same_as<std::ranges::range_value_t<Container>, Entity>
        void destroyEntities(const Container& entities) {
            destroyEntities(std::ranges::begin(entities), std::ranges::end(entities));
        }

        /**
         * @brief Проверить, валидна ли сущность.
         *
         * Сложность: O(1)
         * Потокобезопасность: Безопасно для чтения (если нет параллельных create/destroy).
         */
        [[nodiscard]] bool isAlive(Entity e) const noexcept {
            return mRegistry.valid(e);
        }

        /**
         * @brief Количество живых сущностей в мире.
         *
         * АРХИТЕКТУРНОЕ РЕШЕНИЕ (AAA production standard):
         *  - EnTT НЕ предоставляет публичный O(1) метод для entity count
         *  - registry.valid(entity) - проверяет ОДНУ сущность
         *  - registry.size() - capacity (включая tombstones)
         *  - registry.storage<entity>().size() - implementation detail, нестабильный API
         *  - Итерация registry.each() - O(N), катастрофа при 500k entities
         *
         * РЕШЕНИЕ:
         *  - Собственный счётчик (стандарт Paradox/Unity/Unreal)
         *  - Increment в createEntity(), decrement в flushDestroyed()
         *  - O(1) гарантированно, стабильный API, детерминизм
         *  - Легко расширить (entity count by archetype, component stats)
         *
         * КРИТИЧНО ДЛЯ ПРОФИЛИРОВАНИЯ:
         *  - Ключевая метрика для ECS-нагрузки (особенно при 500k+ entities)
         *  - Используется в DebugOverlay (Profile) и RenderSystem (Debug log)
         *
         * INVARIANT PROTECTION (Debug-only):
         *  - validateEntityCount() периодически проверяет счётчик против реальности
         *  - Детектирует рассинхронизацию (например, прямой вызов registry.destroy)
         *
         * Сложность: O(1)
         * Потокобезопасность: Безопасно для чтения.
         */
        [[nodiscard]] std::size_t aliveEntityCount() const noexcept {
            return mAliveEntityCount;
        }

#if !defined(NDEBUG) || defined(RETRIBUTIO_PROFILE)
        /**
         * @brief Debug/Profile: O(1) storage size for component/tag.
         *
         * Используется только в диагностике (overlay), без аллокаций.
         */
        template <typename T>
        [[nodiscard]] std::size_t debugStorageSize() const noexcept {
            const auto* storage = mRegistry.storage<T>();
            return storage ? storage->size() : 0u;
        }
#endif

        /**
         * @brief Уничтожить все сущности и сбросить счётчики (bulk reset).
         *
         * СЦЕНАРИИ ИСПОЛЬЗОВАНИЯ:
         *  - Level transitions
         *  - Save/load
         *  - Stress test teardown
         *
         * ИНВАРИАНТ: Сбрасывает mAliveEntityCount синхронно с registry.clear().
         *
         * КРИТИЧНО:
         *  - НЕ вызывать registry.clear() напрямую — используйте этот метод.
         *  - Observers on_destroy получат события для всех сущностей.
         *
         * Сложность: O(N), где N — количество компонентов × entities.
         * Потокобезопасность: Небезопасно.
         */
        void clear() {
            mRegistry.clear();
            mAliveEntityCount = 0;
            mDeferredDestroyQueue.clear();
            mStableIds.clear();

#if !defined(NDEBUG)
            // После clear() очередь пуста, но stamp-эпоху обновляем, чтобы "забыть" всё старое
            // без O(N) проходов по маске.
            debugAdvanceQueuedDestroyStamp();
#endif
        }

        /**
         * @brief Выполнить фактическое уничтожение всех отложенных сущностей.
         *
         * Контракт:
         *  - ЕДИНСТВЕННАЯ точка фактического destroy (registry.destroy вызывается только здесь)
         *  - Вызывается ровно один раз на фазовом барьере (после update, до render)
         *  - Запрещено вызывать во время выполнения систем (внутри updateAll)
         *  - Запрещена реентерабельность (вложенный flush)
         *  - on_destroy hooks вызываются здесь
         */
        void flushDestroyed() {
#if !defined(NDEBUG)
            assert(!mDebugUpdateInProgress &&
                   "flushDestroyed: forbidden during World::update() (must be end-of-phase barrier)");
            assert(!mDebugFlushInProgress && "flushDestroyed: recursive call detected");
            ScopedDebugFlag flushGuard(mDebugFlushInProgress);
#endif

            if (mDeferredDestroyQueue.empty()) {
                return;
            }

            const std::size_t count = mDeferredDestroyQueue.size();
            assert(count <= mAliveEntityCount && "flushDestroyed: count exceeds alive entities "
                                                 "(double-destroy or invalid queue)");
#if !defined(NDEBUG)
            for (const Entity e : mDeferredDestroyQueue) {
                assert(isAlive(e) && "flushDestroyed: queued entity is not alive");
            }
#endif
            // ВАЖНО: инвалидация StableID выполняется ПОСЛЕ registry.destroy(), чтобы 
            // on_destroy hooks (вызываемые внутри destroy()) могли ещё прочитать StableID 
            // через tryGet().
            //
            // Безопасно итерировать по "мёртвым" entity handles: entt::to_entity/to_version работают
            // с битовым представлением handle и не обращаются к registry.
            mRegistry.destroy(mDeferredDestroyQueue.begin(), mDeferredDestroyQueue.end());
            for (const Entity e : mDeferredDestroyQueue) {
                mStableIds.onEntityDestroyed(e);
            }
            mDeferredDestroyQueue.clear();
            mAliveEntityCount -= count;

#if !defined(NDEBUG)
            // Новая эпоха: после flush всё считается "не в очереди", без O(N) очисток.
            debugAdvanceQueuedDestroyStamp();
#endif
        }

#if !defined(NDEBUG)
        /**
         * @brief Debug-only: валидация синхронизации счётчика с реальностью.
         *
         * Примечание по дизайну:
         *  - Использует storage<entity>.free_list() — официальный EnTT 3.16 API
         *  - free_list() возвращает количество живых сущностей (индекс границы)
         *  - O(1) — прямой доступ к внутреннему счётчику EnTT
         *  - Вызывается периодически (раз в 60 frames) или перед критическими ops
         *
         * Примечание по имплементации в EnTT 3.16:
         *  - storage<entity> — специализированный storage для entity identifiers
         *  - free_list() — граница между живыми и удалёнными сущностями
         *  - valid(e) реализован как: find(e).index() < free_list()
         *  - const registry.storage<T>() возвращает указатель (может быть nullptr)
         *
         * @return true если счётчик корректен, false если рассинхрон
         */
        [[nodiscard]] bool validateEntityCount() const {
            // EnTT 3.16: const storage<T>() возвращает указатель
            const auto* entityStorage = mRegistry.storage<entt::entity>();

            // Если storage ещё не создан (ни одной сущности не было) — count должен быть 0
            const std::size_t actualCount = entityStorage ? entityStorage->free_list() : 0;

            const bool valid = (actualCount == mAliveEntityCount);

            if (!valid) {
                assert(false && "World::aliveEntityCount desynchronized! "
                                "Direct registry.create/destroy detected or bulk operation bug.");
            }

            return valid;
        }
#endif

        // ----------------------------------------------------------------------------------------
        // Компоненты
        // ----------------------------------------------------------------------------------------

        /**
         * @brief Добавить или заменить компонент (in-place construction).
         *
         * Политика детерминированности:
         *  - addComponent<T>() предназначен ТОЛЬКО для data-компонентов (не empty).
         *  - Для tag-компонентов используйте addTag<T>().
         *  - Для НЕ-пустых компонентов запрещён вызов addComponent<T>(e) без аргументов —
         *    это блокирует случайные "пустые вставки" и скрытые неинициализации.
         *  - Если нужна дефолтная инициализация data-компонента — делай это ЯВНО:
         *      world.addComponent<Health>(e, Health{});
         *
         * Сложность: O(1) amortized.
         * Потокобезопасность: Небезопасно.
         */
        template <DataComponent T, typename... Args>
        requires std::constructible_from<T, Args...> T& addComponent(Entity e, Args&&... args) {
            static_assert(sizeof...(Args) > 0,
                          "World::addComponent<T>: non-empty components must be explicitly "
                          "initialized. Use addComponent<T>(e, T{}) for default initialization.");

            assert(isAlive(e) && "addComponent: entity must be alive");
            return mRegistry.emplace_or_replace<T>(e, std::forward<Args>(args)...);
        }

        /**
         * @brief Добавить или заменить tag-компонент (empty type).
         *
         * Сложность: O(1) amortized.
         * Потокобезопасность: Небезопасно.
         */
        template <TagComponent T> void addTag(Entity e) {
            assert(isAlive(e) && "addTag: entity must be alive");
            mRegistry.emplace_or_replace<T>(e);
        }

        /**
         * @brief Удалить tag-компонент у сущности (если он присутствует).
         *
         * Сложность: O(1)
         * Потокобезопасность: Небезопасно.
         */
        template <TagComponent T>
        void removeTag(Entity e) noexcept(noexcept(mRegistry.remove<T>(e))) {
            assert(isAlive(e) && "removeTag: entity must be alive");
            mRegistry.remove<T>(e);
        }

        /**
         * @brief Проверить наличие tag-компонента у сущности.
         *
         * Контракт: безопасно вызывать с dead/invalid entity — вернёт false.
         */
        template <TagComponent T> [[nodiscard]] bool hasTag(Entity e) const noexcept {
            return mRegistry.any_of<T>(e);
        }

        /**
         * @brief Проверить наличие компонента у сущности.
         *
         * КОНТРАКТ (TRUST ON READ):
         *  - Безопасно вызывать с dead/invalid entity — вернёт false
         *  - EnTT any_of<T>() корректно обрабатывает невалидные entity
         *  - Это критично для hot-path где entity может быть destroyed во время итерации
         *
         * ВАЖНО: Если сразу после проверки нужен доступ к компоненту,
         *        используй tryGetComponent() — это быстрее (один lookup вместо двух).
         *
         * Сложность: O(1)
         * Потокобезопасность: Безопасно для чтения.
         */
        template <Component T>
        [[nodiscard]] bool hasComponent(Entity e) const noexcept {
            // НЕ делать assert(isAlive) — EnTT безопасно возвращает false для невалидных сущностей.
            // Это позволяет использовать hasComponent в hot-path без предварительной проверки.
            return mRegistry.any_of<T>(e);
        }

        /**
         * @brief Безопасное получение: nullptr если сущность невалидна или компонента нет.
         *
         * КОНТРАКТ (ВАЖНО: НЕ МЕНЯТЬ!):
         *  - Безопасно вызывать с dead/invalid entity — вернёт nullptr
         *  - EnTT try_get<T>() корректно обрабатывает невалидные entity
         *  - Это ГАРАНТИЯ EnTT, на которую мы полагаемся в hot-path
         *
         * КРИТИЧНО ДЛЯ ПРОИЗВОДИТЕЛЬНОСТИ:
         *  Делает ОДИН lookup вместо двух при паттерне "проверить и получить":
         *
         *  ❌ Медленно (2 lookup):
         *      if (world.hasComponent<Health>(e)) {
         *          auto& hp = world.getComponent<Health>(e);
         *      }
         *
         *  ✅ Быстро (1 lookup):
         *      if (auto* hp = world.tryGetComponent<Health>(e)) {
         *          hp->value -= damage;
         *      }
         *
         * Сложность: O(1)
         * Потокобезопасность: Небезопасно (эксклюзивный доступ на запись).
         */
        template <Component T> [[nodiscard]] T* tryGetComponent(Entity e) noexcept {
            // КОНТРАКТ: безопасно вызывать на мёртвой сущности; возвращает nullptr (гарантия EnTT).
            // НЕ добавлять assert(isAlive) — это сломает hot-path паттерн.
            return mRegistry.try_get<T>(e);
        }

        /**
         * @brief Константная версия tryGetComponent.
         *
         * CONTRACT: Безопасно вызывать с dead/invalid entity — вернёт nullptr.
         *
         * Сложность: O(1)
         * Потокобезопасность: Безопасно для параллельного чтения.
         */
        template <Component T> [[nodiscard]] const T* tryGetComponent(Entity e) const noexcept {
            // КОНТРАКТ: безопасно вызывать на мёртвой сущности; возвращает nullptr (гарантия EnTT).
            return mRegistry.try_get<T>(e);
        }

        /**
         * @brief Жёсткое получение: assert если сущность невалидна или компонента нет.
         *
         * Реализация через try_get + assert:
         *  - Debug: один lookup и понятное сообщение;
         *  - Release: тот же контракт (не используешь — получишь UB), без лишнего оверхеда.
         *
         * Сложность: O(1)
         * Потокобезопасность: Небезопасно (эксклюзивный доступ на запись).
         */
        template <Component T> [[nodiscard]] T& getComponent(Entity e) {
            assert(isAlive(e) && "getComponent: entity must be alive");
            auto* ptr = mRegistry.try_get<T>(e);
            assert(ptr && "getComponent: component must exist");
            return *ptr;
        }

        /**
         * @brief Константная версия getComponent.
         *
         * Сложность: O(1)
         * Потокобезопасность: Безопасно для параллельного чтения.
         */
        template <Component T> [[nodiscard]] const T& getComponent(Entity e) const {
            assert(isAlive(e) && "getComponent: entity must be alive");
            const auto* ptr = mRegistry.try_get<T>(e);
            assert(ptr && "getComponent: component must exist");
            return *ptr;
        }

        /**
         * @brief Удалить компонент у сущности (если он присутствует).
         *
         * Сложность: O(1)
         * Потокобезопасность: Небезопасно.
         */
        template <Component T>
        void removeComponent(Entity e) noexcept(noexcept(mRegistry.remove<T>(e))) {
            assert(isAlive(e) && "removeComponent: entity must be alive");
            mRegistry.remove<T>(e);
        }

        /**
         * @brief Пометить сущность компонентом-тегом (hot-path для dirty tracking).
         *
         * Примечание по дизайну (TagComponent vs Component):
         *  - Требует TagComponent (не просто is_empty_v<T>)
         *  - Защищает от случайного misuse пустых data-структур
         *  - Явная семантика: это dirty-tag, не компонент с данными
         *
         * Назначение:
         *  - Узкий паттерн для массовых операций внутри view.each().
         *  - T должен быть пустым tag-компонентом (например, SpatialDirtyTag).
         *
         * Ограничения:
         *  - Это исключение, не правило. НЕ добавлять `markFoo()` для каждого workflow.
         *  - Для data-компонентов использовать addComponent<T>(e, data).
         *
         * Сложность: O(1) amortized.
         * Потокобезопасность: Небезопасно.
         */
        template <TagComponent T> void markDirty(Entity e) {
            assert(isAlive(e) && "markDirty: entity must be alive");
            mRegistry.emplace_or_replace<T>(e);
        }

        // ----------------------------------------------------------------------------------------
        // Запросы (view / group)
        // ----------------------------------------------------------------------------------------

        /**
         * @brief EnTT view для итерации по сущностям с заданным набором компонентов.
         *
         * Пример использования:
         *  for (auto [entity, pos, vel] : world.view<Position, Velocity>().each()) {
         *      pos.x += vel.dx * dt;
         *  }
         *
         * Сложность итерации: O(N), где N — количество сущностей с наименьшим компонентом.
         * Потокобезопасность: Небезопасно (если есть параллельные модификации).
         */
        template <Component... Components> [[nodiscard]] auto view() {
            return mRegistry.view<Components...>();
        }

        /**
         * @brief Константный view для read-only систем.
         *
         * КРИТИЧЕСКОЕ ЗАМЕЧАНИЕ ПО ПОТОКОБЕЗОПАСНОСТИ:
         *  - Несмотря на const, этот метод НЕ thread-safe!
         *  - EnTT может мутировать внутренние структуры при формировании view
         *  - const здесь означает "логически read-only", а не "physically const"
         *  - НЕ используйте параллельные вызовы view() без синхронизации
         *
         * ОБОСНОВАНИЕ ДИЗАЙНА:
         *  - Многие AAA ECS вообще не имеют const view (Unity/Unreal)
         *  - Мы оставляем const для удобства read-only систем
         *  - Но контракт честный: это НЕ гарантия thread-safety
         *
         * Сложность итерации: O(N)
         * Потокобезопасность: НЕБЕЗОПАСНО (см. выше).
         */
        template <Component... Components> [[nodiscard]] auto view() const {
            return mRegistry.view<Components...>();
        }

        /**
         * @brief EnTT group для горячих путей (рендер, физика).
         *
         * ВАЖНО:
         *  - Метод намеренно НЕ const.
         *    В EnTT формирование/поддержка group может требовать внутренних мутаций реестра.
         *  - Для read-only кода на const World используй view() const.
         *
         * Пример:
         *  auto group = world.group<Position, Velocity>(entt::get<Sprite>);
         *  // Position и Velocity хранятся плотно, Sprite — отдельно.
         *
         * Сложность: O(1) для получения, O(N) для первой итерации (сортировка).
         * Потокобезопасность: Небезопасно.
         */
        template <Component... Owned, Component... Get>
        [[nodiscard]] auto group(entt::get_t<Get...> get = entt::get_t<Get...>{}) {
            return mRegistry.group<Owned...>(get);
        }

        // ----------------------------------------------------------------------------------------
        // Системы
        // ----------------------------------------------------------------------------------------

        /**
         * @brief Добавить систему в мир.
         *
         * Системы вызываются в порядке добавления.
         */
        template <typename T, typename... Args> T& addSystem(Args&&... args) {
            return mSystems.addSystem<T>(std::forward<Args>(args)...);
        }

        /**
         * @brief Обновить все системы (логическая часть игры).
         */
        void update(float dt) {
#if !defined(NDEBUG)
            ScopedDebugFlag updateGuard(mDebugUpdateInProgress);
#endif
            mSystems.updateAll(*this, dt);
        }

        /**
         * @brief Отрисовать все системы (рендеринг).
         */
        void render(sf::RenderWindow& window) {
            mSystems.renderAll(*this, window);
        }
        
#if !defined(NDEBUG)
        // Debug-only wiring contract:
        // determinismEnabled => StableIdService enabled+prewarmed BEFORE first createEntity().
        void requireStableIdsForDeterminism() noexcept {
            mRequireStableIdsForDeterminism = true;
        }
#endif

        [[nodiscard]] StableIdService& stableIds() noexcept {
            return mStableIds;
        }

        [[nodiscard]] const StableIdService& stableIds() const noexcept {
            return mStableIds;
        }

      private:
        /**
         * @brief Prewarm EnTT entity pool (избегает heap churn в hot-path).
         *
         * АРХИТЕКТУРА:
         *  - Единственная write-boundary для capacity policy
         *  - Вызывается в конструкторе World (cold-path, до любых entity operations)
         *  - Fail-fast при превышении EnTT limit или bad_alloc
         *
         * ЗАЧЕМ:
         *  - Без reserve: каждая 10-я create() вызывает realloc (heap churn, cache miss)
         *  - С reserve: вся память выделяется заранее (zero allocations в hot-path)
         *
         * КРИТИЧНО ДЛЯ ПРОИЗВОДИТЕЛЬНОСТИ:
         *  - 1.6M entities без reserve = ~160 heap allocations за игровой цикл
         *  - 1.6M entities с reserve(1.6M) = 0 heap allocations (вся память из prewarm)
         *
         * КОНТРАКТ:
         *  - reserveEntities == 0 → skip (дефолтное поведение EnTT)
         *  - reserveEntities > EnTT limit → LOG_PANIC (fail-fast, не запускаем игру)
         *  - bad_alloc → LOG_PANIC (не можем работать без памяти)
         *
         * @param reserveEntities Ожидаемое максимальное количество живых entities
         */
        void prewarmRegistry_(std::size_t reserveEntities) noexcept;

        entt::registry mRegistry{};
        SystemManager mSystems{};
        StableIdService mStableIds{};

#if !defined(NDEBUG)
        /**
         * @brief Debug-only RAII для установки/сброса флагов фаз.
         *
         * Это не "защитное программирование": флаги проверяются только в cold write-API
         * (flush/enqueue) и только в Debug. В Release/Profile — ноль.
         */
        struct ScopedDebugFlag {
            explicit ScopedDebugFlag(bool& flagRef) noexcept : flag(flagRef) {
                flag = true;
            }

            ~ScopedDebugFlag() {
                flag = false;
            }

            ScopedDebugFlag(const ScopedDebugFlag&) = delete;
            ScopedDebugFlag& operator=(const ScopedDebugFlag&) = delete;

          private:
            bool& flag;
        };

        [[nodiscard]] static std::size_t debugEntityRawIndex(const Entity e) noexcept {
            return static_cast<std::size_t>(entt::to_entity(e));
        }

        void debugEnsureQueuedStampStorage(const std::size_t rawIndex) {
            if (rawIndex >= mQueuedDestroyStampByIndex.size()) {
                // Debug-only: расширяем таблицу под новые индексы.
                mQueuedDestroyStampByIndex.resize(rawIndex + 1u, 0u);
            }
        }

        void debugMarkQueuedForDestroy(const Entity e) {
            const std::size_t idx = debugEntityRawIndex(e);
            debugEnsureQueuedStampStorage(idx);

            // O(1) детект:
            //  - повторного enqueue (entity уже в очереди в этой эпохе),
            //  - дубликата внутри batch destroyEntities().
            assert(mQueuedDestroyStampByIndex[idx] != mQueuedDestroyStamp &&
                   "destroyDeferred/destroyEntities: entity already queued for destruction");
            mQueuedDestroyStampByIndex[idx] = mQueuedDestroyStamp;
        }

        void debugAdvanceQueuedDestroyStamp() {
            // Переключаем эпоху. 0 зарезервирован как "никогда не маркировали".
            ++mQueuedDestroyStamp;
            if (mQueuedDestroyStamp == 0u) {
                // Переполнение u32 в Debug — крайне редкий случай.
                // Даже при 60 FPS это ~66M секунд (~2 года) непрерывной работы.
                //
                // Сбрасываем всю таблицу в 0 и начинаем с 1.
                // Это O(N) проход по Debug-only таблице; даже для 1M индексов это обычно
                // единичные миллисекунды и происходит "почти никогда".
                for (std::uint32_t& v : mQueuedDestroyStampByIndex) {
                    v = 0u;
                }
                mQueuedDestroyStamp = 1u;
            }
        }
#endif

        // ----------------------------------------------------------------------------------------
        // Диагностика (ECS-метрики)
        // ----------------------------------------------------------------------------------------

        /**
         * @brief Счётчик живых сущностей (AAA production standard).
         *
         * ОБОСНОВАНИЕ ДИЗАЙНА:
         *  - EnTT не предоставляет публичный O(1) метод для entity count
         *  - Собственный счётчик - стандарт индустрии (Paradox/Unity/Unreal)
         *  - Синхронизируется в createEntity()/flushDestroyed()
         *  - Debug: validateEntityCount() проверяет счётчик против реальности
         *
         * INVARIANT PROTECTION:
         *  - Increment ТОЛЬКО в createEntity()
         *  - Decrement ТОЛЬКО в flushDestroyed()
         *  - Reset ТОЛЬКО в clear()
         *  - Debug: assert на underflow (детект lifecycle bugs)
         *
         * СТОИМОСТЬ:
         *  - Memory: 8 bytes (sizeof(std::size_t))
         *  - CPU: negligible (increment/decrement на create/flush)
         *
         * FUTURE EXPANSION:
         *  - Entity count by archetype
         *  - Component pool sizes
         *  - Memory usage stats
         */
        std::size_t mAliveEntityCount{0};

        /**
         * @brief Очередь отложенного уничтожения сущностей.
         *
         * Контракт:
         *  - push_back/insert только в write-фазе
         *  - уничтожение выполняется в flushDestroyed()
         *  - без аллокаций на query/read путях
         *
         * Примечание по производительности:
         *  - Для "no heap churn per destroy" вызывать reserveDeferredDestroyQueue() в cold-пути.
         *  - Здесь нет микро-логики роста: она нарушает DRY и добавляет ветвления в write-API.
         */
        std::vector<Entity> mDeferredDestroyQueue{};

#if !defined(NDEBUG)
        // Debug-only: stamp-таблица для O(1) детекта дубликатов/повторного enqueue.
        // Индексация по raw index entt::entity (entt::to_entity()).
        // Важно:
        //  - raw index может содержать "дырки" (после массовых create/destroy), поэтому размер
        //    таблицы определяется max raw index, а не aliveEntityCount().
        //
        // Memory:
        //  - ~4 bytes × max_entity_raw_index.
        //  - Например, для 1M entities это ≈ 4MB в Debug, что приемлемо для диагностики.
        std::vector<std::uint32_t> mQueuedDestroyStampByIndex{};
        std::uint32_t mQueuedDestroyStamp{1u};

        // Debug-only: минимальная защита от misuse фаз/flush (cold-path asserts).
        bool mDebugUpdateInProgress{false};
        bool mDebugFlushInProgress{false};
        bool mRequireStableIdsForDeterminism{false};
#endif

        // ----------------------------------------------------------------------------------------
        // Прямой доступ к EnTT (только для observers/signals)
        // ----------------------------------------------------------------------------------------

        /**
         * @brief Прямой доступ к entt::registry для специальных механизмов EnTT.
         *
         * Примечание по дизайну:
         *  - registry() намеренно private и доступен ТОЛЬКО избранным core-системам (через friend).
         *  - Game-уровень НИКОГДА не должен напрямую работать с EnTT — только через World API
         *    (createEntity, addComponent, view(), group()).
         *  - Это контролируемый escape hatch для случаев, которые невозможно выразить
         *    через чистый World API без потери производительности или детерминизма.
         *
         * КРИТИЧЕСКОЕ ОГРАНИЧЕНИЕ:
         *  ❗ Friend-классы НЕ должны вызывать registry.create() напрямую
         *  ❗ Friend-классы НЕ должны вызывать registry.destroy() напрямую
         *  ❗ Friend-классы НЕ должны вызывать registry.clear() напрямую
         *
         *  Эти операции ОБЯЗАНЫ идти через World API для синхронизации счётчиков.
         *  Нарушение = рассинхронизация aliveEntityCount и потенциальный crash.
         *
         * Допустимые примеры использования:
         *  - entt::observer / on_destroy<> (реакция на удаление компонентов);
         *  - bulk-операции и hot-path tag-мутации (clear<Tag>(), snapshot);
         *  - read-only доступ к storage для профилирования.
         *
         * Расширение списка friend-классов — архитектурное решение и требует обоснования.
         */
        [[nodiscard]] entt::registry& registry() noexcept {
            return mRegistry;
        }

        [[nodiscard]] const entt::registry& registry() const noexcept {
            return mRegistry;
        }

        // Движковые системы, требующие доступ к registry:
        friend class SpatialIndexSystem; // Требуется on_destroy<> observer
    };

} // namespace core::ecs