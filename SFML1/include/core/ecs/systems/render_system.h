// ================================================================================================
// File: core/ecs/systems/render_system.h
// Purpose: High-performance batched sprite rendering with spatial culling
// Used by: Game layer (render loop)
// Related headers: world.h, spatial_index.h, resource_manager.h
// ================================================================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/Vertex.hpp>
#include <SFML/System/Vector2.hpp>

#include "core/ecs/entity.h"
#include "core/ecs/system.h"
#include "core/resources/ids/resource_ids.h"

namespace sf {
    class RenderWindow;
} // namespace sf

namespace core::resources {
    class ResourceManager;
} // namespace core::resources

namespace core::spatial {
    class SpatialIndex;
} // namespace core::spatial

namespace core::ecs {

    class World;

    /**
     * @brief Высокопроизводительная система рендеринга спрайтов с batching и culling.
     *
     * АРХИТЕКТУРА:
     *  - Spatial culling через SpatialIndex (grid-based, O(visible cells))
     *  - Сортировка по zOrder → textureId → entityId (детерминизм + batching)
     *  - Vertex batching: один draw call на группу (zOrder, texture)
     *  - Zero-allocation hot path (pre-allocated buffers)
     *
     * ПРОИЗВОДИТЕЛЬНОСТЬ (target: 500k entities, 50k visible):
     *  - Gather + cull: ~1ms
     *  - Sort: ~0.5ms
     *  - Build vertices: ~0.5ms
     *  - Draw calls: зависит от GPU, обычно <1ms при batching
     *
     * КРИТИЧНЫЕ ОПТИМИЗАЦИИ:
     *  - mVertexBuffer: raw array без инициализации (make_unique_for_overwrite)
     *  - Amortized growth: capacity растёт с headroom (+50%)
     *  - Per-frame texture cache: избегаем повторных ResourceManager lookups
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
            std::size_t vertexCount{0};              ///< spriteCount × 6
            std::size_t batchDrawCalls{0};           ///< Количество draw calls
            std::size_t textureSwitches{0};          ///< Смены текстуры
            std::size_t uniqueTexturePointers{0};    ///< Уникальные sf::Texture*
            std::size_t textureCacheSize{0};         ///< Размер per-frame кэша
            std::size_t resourceLookupsThisFrame{0}; ///< Обращений к ResourceManager

            std::uint64_t cpuTotalUs{0};  ///< Общее время render() (только SFML1_PROFILE)
            std::uint64_t cpuDrawUs{0};   ///< Время в window.draw() (только SFML1_PROFILE)
            std::uint64_t cpuGatherUs{0}; ///< Время gather + cull (только SFML1_PROFILE)
            std::uint64_t cpuSortUs{0};   ///< Время сортировки (только SFML1_PROFILE)
            std::uint64_t cpuBuildUs{0};  ///< Время построения vertices (только SFML1_PROFILE)
        };

        RenderSystem() = default;
        ~RenderSystem() override = default;

        // Non-copyable, non-movable (содержит raw pointers и large buffers)
        RenderSystem(const RenderSystem&) = delete;
        RenderSystem& operator=(const RenderSystem&) = delete;
        RenderSystem(RenderSystem&&) = delete;
        RenderSystem& operator=(RenderSystem&&) = delete;

        /**
         * @brief Привязать внешние зависимости (обязательно вызвать перед render).
         *
         * LIFETIME CONTRACT:
         *  - spatialIndex и resources должны жить дольше RenderSystem
         *  - Нарушение = dangling pointer, use-after-free
         *
         * @param spatialIndex Указатель на spatial index (не владеем, read-only)
         * @param resources Указатель на resource manager (не владеем, mutable для lazy loading)
         */
        void bind(const core::spatial::SpatialIndex* spatialIndex,
                  core::resources::ResourceManager* resources) noexcept {
            mSpatialIndex = spatialIndex;
            mResources = resources;
        }

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
         * НЕ noexcept: sf::RenderWindow::draw() может выбросить исключение.
         */
        void render(World& world, sf::RenderWindow& window) override;

        /**
         * @brief Получить статистику последнего кадра.
         *
         * @return FrameStats с метриками (zeroed в Release без SFML1_PROFILE)
         */
        [[nodiscard]] FrameStats getLastFrameStats() const noexcept;

      private:
        // ------------------------------------------------------------------------------------
        // Внешние зависимости (не владеем, lifetime managed externally)
        // ------------------------------------------------------------------------------------

        /// Spatial index для view-frustum culling (read-only, cell-level candidates)
        const core::spatial::SpatialIndex* mSpatialIndex{nullptr};

        /// Resource manager для резолва TextureID → sf::Texture.
        /// НЕ const: getTexture() использует lazy loading и обновляет внутренние кэши.
        core::resources::ResourceManager* mResources{nullptr};

        // ------------------------------------------------------------------------------------
        // Внутренние структуры (горячий путь)
        // ------------------------------------------------------------------------------------

        /**
         * @brief Ключ сортировки для детерминированного batching.
         *
         * Порядок сортировки: zOrder → textureId → tieBreak (entity ID).
         * Минимизирует texture switches и гарантирует детерминизм.
         */
        struct RenderKey {
            float zOrder;                              ///< Слой отрисовки
            core::resources::ids::TextureID textureId; ///< ID текстуры для batching
            std::uint32_t tieBreak;                    ///< Entity ID для стабильной сортировки
            std::uint32_t packetIndex;                 ///< Индекс в mPackets (uint32_t достаточно)
        };
        static_assert(sizeof(RenderKey) == 16, "RenderKey must be 16 bytes for cache efficiency");

        /**
         * @brief Данные для генерации вершин одного спрайта.
         *
         * Собираются один раз при gather, используются при build vertices.
         * Sin/cos кэшируются чтобы не считать дважды.
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

        /**
         * @brief Per-frame кэш: TextureID → sf::Texture*.
         *
         * Избегаем повторных обращений к ResourceManager за один кадр.
         * Очищается в начале каждого render().
         */
        struct TextureCacheEntry {
            core::resources::ids::TextureID id{core::resources::ids::TextureID::Unknown};
            const sf::Texture* texture{nullptr};
        };

        // ------------------------------------------------------------------------------------
        // Буферы (amortized growth, переиспользуются между кадрами)
        // ------------------------------------------------------------------------------------

        /// Результат SpatialIndex::query() — кандидаты на рендеринг
        std::vector<Entity> mVisibleEntities;

        /// Ключи сортировки (по одному на visible sprite)
        std::vector<RenderKey> mKeys;

        /// Данные для генерации вершин (по одному на visible sprite)
        std::vector<RenderPacket> mPackets;

        /**
         * @brief Vertex buffer (raw array, NO initialization).
         *
         * КРИТИЧНАЯ ОПТИМИЗАЦИЯ:
         *  - std::vector<sf::Vertex>::resize() зануляет память
         *  - std::unique_ptr<sf::Vertex[]> + make_unique_for_overwrite = zero init cost
         *  - Записываем напрямую в mVertexBuffer.get() + offset
         *  - sf::RenderWindow::draw() принимает raw pointer + count
         */
        std::unique_ptr<sf::Vertex[]> mVertexBuffer{};
        std::size_t mVertexBufferCapacity{0};

        /// Per-frame texture cache (очищается каждый кадр)
        std::vector<TextureCacheEntry> mTextureCache;

        /// Hint для amortized growth (последнее количество visible)
        std::size_t mLastVisibleCount{0};

        // ------------------------------------------------------------------------------------
        // Статистика (Debug/Profile only)
        // ------------------------------------------------------------------------------------

#if !defined(NDEBUG) || defined(SFML1_PROFILE)
        FrameStats mLastStats{};

        /// Для подсчёта уникальных sf::Texture* за кадр
        std::vector<const sf::Texture*> mUniqueTexturePointers;
#endif

        // ------------------------------------------------------------------------------------
        // Debug logging state (члены класса вместо static — избегаем race condition)
        // ------------------------------------------------------------------------------------

#if !defined(NDEBUG)
        /**
         * @brief Счётчик кадров для throttled debug logging.
         *
         * ВАЖНО: не static!
         *  - static в функции живёт вечно, shared между всеми instances
         *  - При многопоточности (будущее) = data race
         *  - Член класса = isolated per-instance state
         *
         * std::uint64_t вместо int:
         *  - Signed overflow = UB (компилятор может "оптимизировать")
         *  - uint64 overflow через триллионы лет
         */
        std::uint64_t mDebugFrameCount{0};

        /// Последнее залогированное количество visible (для change detection)
        std::size_t mDebugLastLoggedCount{static_cast<std::size_t>(-1)};
#endif
    };

} // namespace core::ecs