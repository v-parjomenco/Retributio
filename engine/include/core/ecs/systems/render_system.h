// ================================================================================================
// File: core/ecs/systems/render_system.h
// Purpose: High-performance batched sprite rendering with spatial culling
// Used by: Game layer (render loop)
// Related headers: world.h, spatial_index_v2.h, resource_manager.h
// ================================================================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/Vertex.hpp>
#include <SFML/System/Vector2.hpp>

#include "core/ecs/entity.h"
#include "core/ecs/system.h"
#include "core/resources/keys/resource_key.h"
#include "core/spatial/spatial_index_v2.h"

namespace sf {
    class RenderWindow;
    class Texture;
} // namespace sf

namespace core::resources {
    class ResourceManager;
} // namespace core::resources

namespace core::ecs {

    class World;

    /**
     * @brief Высокопроизводительная система рендеринга спрайтов с batching и culling.
     *
     * АРХИТЕКТУРА И ПРОИЗВОДИТЕЛЬНОСТЬ:
     *  - Spatial culling через SpatialIndex (grid-based, O(visible cells)).
     *  - Детерминированная сортировка: zOrder -> texture -> entityId (batching + стабильность).
     *  - Vertex batching: один draw call на группу (zOrder, texture).
     *  - Zero-allocation hot path: буферы переиспользуются (amortized growth).
     *  - Per-frame cache текстур: epoch-массивы по key.index() (O(1), без map/хешей).
     *
     * КОНТРАКТ:
     *  - Разрешение TextureKey -> sf::Texture выполняется через ResourceManager.
     *  - Missing texture key должен быть валиден.
     */
    class RenderSystem final : public ISystem {
      public:
        /**
         * @brief Статистика кадра для профилирования и debug overlay.
         *
         * Debug/Profile builds: все поля заполняются.
         * Release builds: возвращается zeroed struct (zero overhead).
         */
        struct FrameStats {
            std::size_t totalSpriteCount{0};         ///< Кандидаты от SpatialIndex
            std::size_t culledSpriteCount{0};        ///< Отсечено fine culling
            std::size_t spriteCount{0};              ///< Фактически отрисовано
            std::size_t vertexCount{0};              ///< spriteCount * 6
            std::size_t batchDrawCalls{0};           ///< Количество draw calls
            std::size_t textureSwitches{0};          ///< Смены текстуры (batched flush)
            std::size_t uniqueTexturePointers{0};    ///< Уникальные текстуры за кадр по epoch cache
            std::size_t textureCacheSize{0};         ///< Размер per-frame epoch cache
            std::size_t resourceLookupsThisFrame{0}; ///< Уникальные resident-fetch по texture key
            std::size_t renderMapNull{0};            ///< id->NullEntity
            std::size_t renderMissingComponents{0};  ///< !ecsView.contains
            std::size_t renderFineCullFail{0};       ///< lastAabb не пересёк viewAabb

            std::uint64_t cpuTotalUs{0};  ///< Общее время render() (только RETRIBUTIO_PROFILE)
            std::uint64_t cpuDrawUs{0};   ///< Время в window.draw() (только RETRIBUTIO_PROFILE)
            std::uint64_t cpuGatherUs{0}; ///< Время gather + cull (только RETRIBUTIO_PROFILE)
            std::uint64_t cpuSortUs{0};   ///< Время сортировки (только RETRIBUTIO_PROFILE)
            std::uint64_t cpuBuildUs{0};  ///< Время построения vertices (только RETRIBUTIO_PROFILE)
        };

        RenderSystem() = default;
        ~RenderSystem() override = default;

        // Non-copyable, non-movable (содержит raw pointers и large buffers)
        RenderSystem(const RenderSystem&) = delete;
        RenderSystem& operator=(const RenderSystem&) = delete;
        RenderSystem(RenderSystem&&) = delete;
        RenderSystem& operator=(RenderSystem&&) = delete;

        /**
         * @brief Привязать внешние зависимости (обязательно вызвать перед render()).
         *
         * Контракт времени жизни:
         *  - spatialIndex и resources должны жить дольше RenderSystem.
         *
         * Контракт инициализации (fail-fast):
         *  - resources уже инициализирован (key-world): registry загружен и валиден.
         *  - resources->registry().textureCount() > 0
         *  - resources->missingTextureKey().valid()
         *  - resources->missingTextureKey().index() < textureCount
         *
         * bind() подготавливает epoch-массивы под текущий размер registry.
         * При hot-reload registry bind() должен быть вызван повторно.
         *
         * Дополнительно:
         *  - bind(nullptr, nullptr) допустим как явный "unbind" (сбрасывает кэш и состояние).
         *  - bind(spatialIndex, nullptr) / bind(nullptr, resources) — misuse -> (LOG_PANIC).
         *  - entitiesBySpatialId must be non-empty when spatialIndex is provided.
         *  - maxVisibleSprites > 0: upper bound for query output and vertex buffer sizing.
         */
        void bind(const core::spatial::SpatialIndexV2Sliding* spatialIndex,
                  std::span<const Entity> entitiesBySpatialId, std::size_t maxVisibleSprites,
                  const core::resources::ResourceManager* resources);

        /**
         * @brief Логическое обновление (не используется, рендер-only система).
         */
        void update(World& world, float dt) override {
            (void) world;
            (void) dt;
        }

        /**
         * @brief Отрисовка всех видимых спрайтов с batching.
         *
         * НЕ noexcept: sf::RenderWindow::draw() не гарантирует noexcept.
         */
        void render(World& world, sf::RenderWindow& window) override;

        /**
         * @brief Получить статистику последнего кадра.
         *
         * @return Ссылка на FrameStats (в Release возвращается zeroed static).
         */
        [[nodiscard]] const FrameStats& getLastFrameStatsRef() const noexcept;

      private:
        void beginFrame() noexcept;

        [[nodiscard]] const sf::Texture* getTextureCached(core::resources::TextureKey key,
                                                          std::size_t* resourceLookupsThisFrame);

        void resizeEpochArrays(std::size_t textureCount);

        // ----------------------------------------------------------------------------------------
        // Внешние зависимости (не владеем, lifetime managed externally)
        // ----------------------------------------------------------------------------------------

        /// Spatial index для view-frustum culling (read-only, cell-level candidates)
        const core::spatial::SpatialIndexV2Sliding* mSpatialIndex{nullptr};

        /// O(1) mapping from SpatialId32 -> Entity (index = id).
        const Entity* mEntitiesBySpatialId{nullptr};
        std::size_t mEntitiesBySpatialIdSize{0};

        /// Resource manager (resident-only access in render; NO lazy-load allowed).
        const core::resources::ResourceManager* mResources{nullptr};

        // ----------------------------------------------------------------------------------------
        // Внутренние структуры (горячий путь)
        // ----------------------------------------------------------------------------------------

        /**
         * @brief Ключ сортировки для детерминированного batching.
         * Порядок: zOrder -> texture -> tieBreak (entity ID).
         * Минимизирует переключения текстур и гарантирует стабильность кадра.
         */
        struct RenderKey {
            float zOrder;
            core::resources::TextureKey texture;
            std::uint32_t tieBreak;
            std::uint32_t packetIndex;
        };
        static_assert(sizeof(RenderKey) == 16, "RenderKey must be 16 bytes for cache efficiency");

        /**
         * @brief Данные для генерации вершин одного спрайта.
         * Собираются один раз при gather, используются при build. Sin/cos кэшируются.
         */
        struct RenderPacket {
            sf::Vector2f position;
            sf::Vector2f origin;
            sf::Vector2f scale;
            sf::IntRect rect;
            float cachedSin;
            float cachedCos;
            bool isRotated;
        };

        // ----------------------------------------------------------------------------------------
        // Буферы (amortized growth, переиспользуются между кадрами)
        // ----------------------------------------------------------------------------------------

        std::vector<core::spatial::EntityId32> mQueryBuffer;
        std::vector<core::spatial::EntityId32> mVisibleIds;
        std::size_t mVisibleCount{0};
        std::size_t mMaxVisibleSprites{0};
        std::vector<RenderKey> mKeys;
        std::vector<RenderPacket> mPackets;

        /**
         * @brief Vertex buffer (raw array, NO initialization).
         *
         * КРИТИЧНАЯ ОПТИМИЗАЦИЯ:
         *  - std::vector<sf::Vertex>::resize() зануляет память (~7MB на 50k спрайтов).
         *  - std::unique_ptr<sf::Vertex[]> + make_unique_for_overwrite = zero init cost.
         *  - sf::RenderWindow::draw() принимает raw pointer + count.
         */
        std::unique_ptr<sf::Vertex[]> mVertexBuffer{};
        std::size_t mVertexBufferCapacity{0};

        /// Per-frame texture cache (epoch arrays)
        std::vector<const sf::Texture*> mFrameTexturePtr;
        std::vector<std::uint32_t> mFrameTextureStamp;
        std::uint32_t mFrameId{0};
        std::uint32_t mCachedResourceGen{0};

        /// Hint для amortized growth (последнее количество visible)
        std::size_t mLastVisibleCount{0};

        // ----------------------------------------------------------------------------------------
        // Статистика (Debug/Profile only)
        // ----------------------------------------------------------------------------------------

#if !defined(NDEBUG) || defined(RETRIBUTIO_PROFILE)
        FrameStats mLastStats{};
        std::size_t mUniqueTexturesThisFrame{0};
#endif

#if !defined(NDEBUG)
        /**
         * @brief Счётчик кадров для throttled debug logging.
         *
         * ВАЖНО: не static!
         *  - static в функции создаёт shared state между всеми экземплярами (data race).
         *  - Член класса обеспечивает изоляцию.
         *  - uint64 для защиты от переполнения.
         */
        std::uint64_t mDebugFrameCount{0};
        std::size_t mDebugLastLoggedCount{static_cast<std::size_t>(-1)};
#endif
    };

} // namespace core::ecs