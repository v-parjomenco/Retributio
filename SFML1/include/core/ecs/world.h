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
#include <iterator>
#include <ranges>
#include <type_traits>
#include <utility>

#include "third_party/entt/entt_registry.hpp"

#include "core/ecs/entity.h"
#include "core/ecs/system_manager.h"

namespace sf {
    class RenderWindow;
} // namespace sf

namespace core::ecs {

    /**
     * @brief Политика допустимого ECS-компонента (compile-time фильтр).
     *
     * Требования проекта:
     *  - POD-like: trivially copyable (SoA-friendly, быстрая сериализация);
     *  - "Голый" тип: без const/volatile и без ссылок (EnTT хранит не-cvref типы);
     *  - Без владения ресурсами: trivially destructible (никаких нетривиальных деструкторов).
     *
     * DESIGN NOTE (strictness):
     *  - Этот концепт намеренно СТРОГИЙ для core ECS компонентов.
     *  - Для save/load могут потребоваться relaxed компоненты с custom ctor.
     *  - Такие случаи должны использовать отдельный SaveComponent concept.
     */
    template <typename T>
    concept Component = std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T> &&
                        !std::is_const_v<T> && !std::is_volatile_v<T> && !std::is_reference_v<T>;

    /**
     * @brief Политика tag-компонента (пустой маркер для dirty tracking).
     *
     * DESIGN NOTE:
     *  - std::is_empty_v<T> недостаточно строго: любой пустой struct пройдёт.
     *  - TagComponent явно указывает семантику: это маркер, не data.
     *  - Используется для ограничения World::markDirty<T> (защита от misuse).
     *
     * Требования:
     *  - Базовый компонент (trivially copyable etc.)
     *  - Пустой тип (no data fields)
     *
     * Примеры:
     *  ✅ struct SpatialDirtyTag {};
     *  ❌ struct Health { int value; };  // Не пустой
     *  ❌ struct NetworkDirty {};         // Пустой, но не intended как tag (нужен explicit opt-in)
     */
    template <typename T> concept TagComponent = Component<T> && std::is_empty_v<T>;

    /**
     * @brief Главный координатор ECS (тонкий фасад над entt::registry).
     *
     * Цели дизайна:
     *  - Zero overhead: всё инлайнится и сводится к прямым вызовам EnTT;
     *  - Fail-fast в Debug: assert на инварианты, нулевой cost в Release;
     *  - Жёсткая политика компонентов: концепт Component режет неподходящие типы на компиляции.
     *
     * КРИТИЧНЫЕ ИНВАРИАНТЫ (entity lifecycle):
     *  1. createEntity() — единственный способ создания сущностей
     *  2. destroyEntity() — единственный способ уничтожения сущностей
     *  3. mAliveEntityCount синхронизирован с registry (Debug: validateEntityCount)
     *  4. registry() доступен ТОЛЬКО через friend (escape hatch)
     *  5. Friend-классы НЕ должны вызывать registry.create/destroy напрямую
     */
    class World {
      public:
        World() = default;
        ~World() = default;

        // World — единственный владелец реестра, копирование запрещаем.
        World(const World&) = delete;
        World& operator=(const World&) = delete;

        // ----------------------------------------------------------------------------------------
        // Move semantics (CRITICAL: manual implementation required)
        // ----------------------------------------------------------------------------------------

        /**
         * @brief Move constructor (ручная реализация для корректного переноса счётчика).
         *
         * CRITICAL BUG FIX:
         *  - Default move копирует примитивные типы (mAliveEntityCount)
         *  - После std::move(w1) старый объект w1 остаётся с mAliveEntityCount > 0
         *  - Но registry пуст → рассинхронизация!
         *
         * РЕШЕНИЕ:
         *  - Ручная реализация с обнулением счётчика у источника
         *  - Источник после move остаётся в валидном moved-from состоянии (count=0)
         *
         * INVARIANT:
         *  - После move: source.aliveEntityCount() == 0
         *  - После move: dest.aliveEntityCount() == old(source.aliveEntityCount())
         *
         * @param other Источник перемещения (будет обнулён)
         */
        World(World&& other) noexcept
            : mRegistry(std::move(other.mRegistry)), mSystems(std::move(other.mSystems)),
              mAliveEntityCount(other.mAliveEntityCount) {
            other.mAliveEntityCount = 0; // CRITICAL: обнуляем источник
        }

        /**
         * @brief Move assignment (ручная реализация для корректного переноса счётчика).
         *
         * CRITICAL BUG FIX:
         *  - Default move-assign копирует примитивы → рассинхронизация
         *
         * РЕШЕНИЕ:
         *  - Ручная реализация с обнулением счётчика у источника
         *  - Self-assignment защита (if this != &other)
         *
         * INVARIANT:
         *  - После move: source.aliveEntityCount() == 0
         *  - После move: this->aliveEntityCount() == old(source.aliveEntityCount())
         *
         * @param other Источник перемещения (будет обнулён)
         * @return *this
         */
        World& operator=(World&& other) noexcept {
            if (this != &other) {
                mRegistry = std::move(other.mRegistry);
                mSystems = std::move(other.mSystems);
                mAliveEntityCount = other.mAliveEntityCount;
                other.mAliveEntityCount = 0; // CRITICAL: обнуляем источник
            }
            return *this;
        }

        // ----------------------------------------------------------------------------------------
        // Жизненный цикл сущностей
        // ----------------------------------------------------------------------------------------

        /**
         * @brief Создать новую сущность.
         *
         * INVARIANT: Increment mAliveEntityCount (для O(1) diagnostics).
         *
         * Сложность: O(1)
         * Thread-safety: Небезопасно.
         */
        [[nodiscard]] Entity createEntity() {
            ++mAliveEntityCount;
            return mRegistry.create();
        }

        /**
         * @brief Уничтожить сущность и все её компоненты.
         *
         * INVARIANT: Decrement mAliveEntityCount (поддерживаем синхронизацию).
         *
         * Сложность: O(N), где N — количество типов компонентов у сущности.
         * Thread-safety: Небезопасно.
         */
        void destroyEntity(Entity e) {
            assert(isAlive(e) && "destroyEntity: entity must be alive");
            assert(mAliveEntityCount > 0 &&
                   "destroyEntity: counter underflow (bug in entity lifecycle)");

            mRegistry.destroy(e);
            --mAliveEntityCount;
        }

        /**
         * @brief Уничтожить несколько сущностей за одну операцию (batch destroy).
         *
         * КРИТИЧНО ДЛЯ ПРОИЗВОДИТЕЛЬНОСТИ:
         *  - Для 500k+ entities удаление 50k сущностей в цикле destroyEntity() медленно
         *  - Batch destroy использует EnTT оптимизацию (одна операция вместо N)
         *  - Типичный use case: удаление всех юнитов фракции после поражения
         *
         * INVARIANT: Decrement mAliveEntityCount на count сущностей.
         *
         * ТРЕБОВАНИЯ:
         *  - Все сущности в диапазоне должны быть валидны (isAlive)
         *  - Observers on_destroy получат события для всех сущностей
         *
         * @warning НЕ передавайте итераторы напрямую из registry.view() или registry.each()!
         *          EnTT может инвалидировать их во время удаления.
         *          Всегда копируйте в std::vector<Entity> перед вызовом.
         *
         * @pre [first, last) должен быть СТАБИЛЬНЫМ диапазоном (std::vector, std::array, std::span).
         *      Передача итераторов EnTT view = потенциальный UB.
         * @pre ALL entities in [first, last) MUST be alive (isAlive() == true).
         *      Passing destroyed/recycled entities = UB in Release, assert in Debug.
         *      Caller is responsible for not storing "stale" entity IDs after destroy.
         *
         * @tparam Iterator ForwardIterator над Entity
         * @param first Начало диапазона
         * @param last Конец диапазона (exclusive)
         *
         * Сложность: O(N × M), где N — количество сущностей, M — средние компоненты.
         *            Но быстрее чем N вызовов destroyEntity() из-за EnTT batch optimization.
         * Thread-safety: Небезопасно.
         */
        template <typename Iterator> void destroyEntities(Iterator first, Iterator last) {
            // FIX: std::distance может вернуть отрицательное значение при некорректных итераторах.
            // static_cast<std::size_t> от отрицательного числа = огромное положительное → underflow
            // в mAliveEntityCount.
            const auto signedDist = std::distance(first, last);
            assert(signedDist >= 0 && "destroyEntities: invalid iterator range (last < first)");

            const std::size_t count = static_cast<std::size_t>(signedDist);

            assert(count <= mAliveEntityCount && "destroyEntities: count exceeds alive entities "
                                                 "(invalid iterators or double-destroy)");

#if !defined(NDEBUG)
            // Debug: проверяем что все сущности валидны
            for (auto it = first; it != last; ++it) {
                assert(isAlive(*it) && "destroyEntities: iterator contains invalid entity");
            }
#endif

            // EnTT batch destroy (гораздо быстрее чем цикл)
            mRegistry.destroy(first, last);
            mAliveEntityCount -= count;
        }

        /**
         * @brief Уничтожить несколько сущностей из контейнера (convenience overload).
         *
         * КРИТИЧНО ДЛЯ ПРОИЗВОДИТЕЛЬНОСТИ:
         *  - std::span/vector → batch destroy
         *  - Contiguous memory = cache-friendly iteration
         *
         * ОГРАНИЧЕНИЕ (compile-time):
         *  - Требует contiguous_range (vector, array, span)
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
         * Thread-safety: Небезопасно.
         */
        template <typename Container>
            requires std::ranges::contiguous_range<Container>
        void destroyEntities(const Container& entities) {
            destroyEntities(std::begin(entities), std::end(entities));
        }

        /**
         * @brief Проверить, валидна ли сущность.
         *
         * Сложность: O(1)
         * Thread-safety: Безопасно для чтения (если нет параллельных create/destroy).
         */
        [[nodiscard]] bool isAlive(Entity e) const noexcept(noexcept(mRegistry.valid(e))) {
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
         *  - Increment в createEntity(), decrement в destroyEntity()
         *  - O(1) гарантированно, stable API, детерминизм
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
         * Thread-safety: Безопасно для чтения.
         */
        [[nodiscard]] std::size_t aliveEntityCount() const noexcept {
            return mAliveEntityCount;
        }

        /**
         * @brief Уничтожить все сущности и сбросить счётчики (bulk reset).
         *
         * USE CASES:
         *  - Level transitions
         *  - Save/load
         *  - Stress test teardown
         *
         * INVARIANT: Сбрасывает mAliveEntityCount синхронно с registry.clear().
         *
         * CRITICAL:
         *  - НЕ вызывать registry.clear() напрямую — используйте этот метод.
         *  - Observers on_destroy получат события для всех сущностей.
         *
         * Сложность: O(N), где N — количество компонентов × entities.
         * Thread-safety: Небезопасно.
         */
        void clear() {
            mRegistry.clear();
            mAliveEntityCount = 0;
        }

#if !defined(NDEBUG)
        /**
         * @brief Debug-only: валидация синхронизации счётчика с реальностью.
         *
         * DESIGN:
         *  - Использует storage<entity>.free_list() — официальный EnTT 3.16 API
         *  - free_list() возвращает количество живых сущностей (индекс границы)
         *  - O(1) — прямой доступ к внутреннему счётчику EnTT
         *  - Вызывается периодически (раз в 60 frames) или перед критическими ops
         *
         * EnTT 3.16 IMPLEMENTATION NOTE:
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
         *  - Для НЕ-пустых компонентов запрещён вызов addComponent<T>(e) без аргументов.
         *    Это блокирует случайные "пустые вставки" и скрытые неинициализации.
         *  - Для tag-компонентов (empty type) addComponent<Tag>(e) разрешён.
         *  - Если нужна дефолтная инициализация data-компонента — делай это ЯВНО:
         *      world.addComponent<Health>(e, Health{});
         *
         * Сложность: O(1) amortized.
         * Thread-safety: Небезопасно.
         */
        template <Component T, typename... Args>
        requires std::constructible_from<T, Args...> T& addComponent(Entity e, Args&&... args) {
            static_assert(sizeof...(Args) > 0 || std::is_empty_v<T>,
                          "World::addComponent<T>: non-empty components must be explicitly "
                          "initialized. Use addComponent<T>(e, T{}) for default initialization.");

            assert(isAlive(e) && "addComponent: entity must be alive");
            return mRegistry.emplace_or_replace<T>(e, std::forward<Args>(args)...);
        }

        /**
         * @brief Добавить или заменить tag-компонент (empty type).
         *
         * Используется, когда addComponent<T>() возвращает void для empty типов в EnTT.
         *
         * Сложность: O(1) amortized.
         * Thread-safety: Небезопасно.
         */
        template <TagComponent T> void addTagComponent(Entity e) {
            assert(isAlive(e) && "addTagComponent: entity must be alive");
            mRegistry.emplace_or_replace<T>(e);
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
         * Thread-safety: Безопасно для чтения.
         */
        template <Component T>
        [[nodiscard]] bool hasComponent(Entity e) const noexcept {
            // NO assert(isAlive) — EnTT safely returns false for invalid entities.
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
         * Thread-safety: Небезопасно (эксклюзивный доступ на запись).
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
         * Thread-safety: Безопасно для параллельного чтения.
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
         * Thread-safety: Небезопасно (эксклюзивный доступ на запись).
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
         * Thread-safety: Безопасно для параллельного чтения.
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
         * Thread-safety: Небезопасно.
         */
        template <Component T>
        void removeComponent(Entity e) noexcept(noexcept(mRegistry.remove<T>(e))) {
            assert(isAlive(e) && "removeComponent: entity must be alive");
            mRegistry.remove<T>(e);
        }

        /**
         * @brief Пометить сущность компонентом-тегом (hot-path для dirty tracking).
         *
         * DESIGN NOTE (TagComponent vs Component):
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
         * Thread-safety: Небезопасно.
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
         * Thread-safety: Небезопасно (если есть параллельные модификации).
         */
        template <Component... Components> [[nodiscard]] auto view() {
            return mRegistry.view<Components...>();
        }

        /**
         * @brief Константный view для read-only систем.
         *
         * CRITICAL THREAD-SAFETY NOTE:
         *  - Несмотря на const, этот метод НЕ thread-safe!
         *  - EnTT может мутировать внутренние структуры при формировании view
         *  - const здесь означает "логически read-only", а не "physically const"
         *  - НЕ используйте параллельные вызовы view() без синхронизации
         *
         * DESIGN RATIONALE:
         *  - Многие AAA ECS вообще не имеют const view (Unity/Unreal)
         *  - Мы оставляем const для удобства read-only систем
         *  - Но контракт честный: это НЕ гарантия thread-safety
         *
         * Сложность итерации: O(N)
         * Thread-safety: НЕБЕЗОПАСНО (см. выше).
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
         * Thread-safety: Небезопасно.
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
            mSystems.updateAll(*this, dt);
        }

        /**
         * @brief Отрисовать все системы (рендеринг).
         */
        void render(sf::RenderWindow& window) {
            mSystems.renderAll(*this, window);
        }

      private:
        entt::registry mRegistry{};
        SystemManager mSystems{};

        // ----------------------------------------------------------------------------------------
        // Диагностика (ECS metrics)
        // ----------------------------------------------------------------------------------------

        /**
         * @brief Счётчик живых сущностей (AAA production standard).
         *
         * DESIGN RATIONALE:
         *  - EnTT не предоставляет публичный O(1) метод для entity count
         *  - Собственный счётчик - стандарт индустрии (Paradox/Unity/Unreal)
         *  - Синхронизируется в createEntity()/destroyEntity()
         *  - Debug: validateEntityCount() проверяет счётчик против реальности
         *
         * INVARIANT PROTECTION:
         *  - Increment ТОЛЬКО в createEntity()
         *  - Decrement ТОЛЬКО в destroyEntity()
         *  - Reset ТОЛЬКО в clear()
         *  - Debug: assert на underflow (детект lifecycle bugs)
         *
         * COST:
         *  - Memory: 8 bytes (sizeof(std::size_t))
         *  - CPU: negligible (increment/decrement на create/destroy)
         *
         * FUTURE EXPANSION:
         *  - Entity count by archetype
         *  - Component pool sizes
         *  - Memory usage stats
         */
        std::size_t mAliveEntityCount{0};

        // ----------------------------------------------------------------------------------------
        // Прямой доступ к EnTT (только для observers/signals)
        // ----------------------------------------------------------------------------------------

        /**
         * @brief Прямой доступ к entt::registry для специальных механизмов EnTT.
         *
         * DESIGN NOTE:
         *  - registry() намеренно private и доступен ТОЛЬКО избранным core-системам (через friend).
         *  - Game-уровень НИКОГДА не должен напрямую работать с EnTT — только через World API
         *    (createEntity, addComponent, view(), group()).
         *  - Это контролируемый escape hatch для случаев, которые невозможно выразить
         *    через чистый World API без потери производительности или детерминизма.
         *
         * CRITICAL RESTRICTION:
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