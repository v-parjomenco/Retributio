    // ================================================================================================
    // File: core/ecs/systems/render_system.h
    // Purpose: Production render system with ID-based sprites, deterministic ordering, batching-ready.
    // Used by: World/SystemManager
    // Related headers: sprite_component.h, transform_component.h, resource_manager.h
    // ================================================================================================
    #pragma once

    #include <cstddef>
    #include <cstdint>
    #include <utility>
    #include <vector>

    #include <SFML/Graphics/Rect.hpp>
    #include <SFML/Graphics/RenderWindow.hpp>
    #include <SFML/Graphics/Texture.hpp>
    #include <SFML/Graphics/Vertex.hpp>

    #include "core/ecs/entity.h"
    #include "core/ecs/system.h"
    #include "core/resources/ids/resource_ids.h"
    #include "core/resources/resource_manager.h"

    namespace core::ecs {

        class RenderSystem final : public ISystem {
          public:
            explicit RenderSystem(core::resources::ResourceManager& resources)
                : mResources(&resources) {
                // Ожидаемо маленькое число уникальных текстур в кадре (десятки).
                // Reserve делаем один раз, чтобы исключить аллокации в горячем пути.
                mTextureCache.reserve(kExpectedUniqueTexturesPerFrame);
    #if !defined(NDEBUG) || defined(SFML1_PROFILE)
                mUniqueTexturePointers.reserve(kExpectedUniqueTexturesPerFrame);
    #endif
            }

            void render(World& world, sf::RenderWindow& window) override;

            struct FrameStats {
                // Общее число спрайтов-кандидатов, рассмотренных на фазе Gather (до culling).
                std::size_t totalSpriteCount = 0;
                // Число отсечённых culling-ом спрайтов-кандидатов (спрайты вне view + invalid).
                std::size_t culledSpriteCount = 0;

                // Число реально отрисованных спрайтов (после culling).
                std::size_t spriteCount = 0;
                std::size_t vertexCount = 0;
                std::size_t batchDrawCalls = 0;

                // Сколько раз сменилась текстура в батче.
                std::size_t textureSwitches = 0;
                // Сколько уникальных sf::Texture* реально использовано в кадре.
                std::size_t uniqueTexturePointers = 0;

                // Debug/diagnostics: размер per-frame TextureID cache.
                std::size_t textureCacheSize = 0;
                // Debug/diagnostics: сколько раз в этом кадре был "slow-path" резолв
                // TextureID -> sf::Texture через ResourceManager (cache miss).
                std::size_t resourceLookupsThisFrame = 0;

                // Профилирование CPU — только для SFML1_PROFILE.
                std::uint64_t cpuTotalUs = 0;
                std::uint64_t cpuDrawUs = 0;

                // Детализация CPU-стоимости (заполняется только в SFML1_PROFILE).
                std::uint64_t cpuGatherUs = 0;
                std::uint64_t cpuSortUs = 0;
                std::uint64_t cpuBuildUs = 0;
            };

            [[nodiscard]] FrameStats getLastFrameStats() const noexcept;

          private:
            core::resources::ResourceManager* mResources;

            // Ожидаемо маленькое число уникальных текстур в кадре (десятки).
            // Держим один общий лимит, чтобы и Release работал без аллокаций на кэше прямоугольников.
            static constexpr std::size_t kExpectedUniqueTexturesPerFrame = 64;

            // Лёгкий ключ сортировки.
            // Сортируем это (дёшево), а тяжёлые данные читаем из ECS только в фазе Draw.
            struct RenderKey {
                float zOrder{};
                core::resources::ids::TextureID textureId{};
                Entity entity{NullEntity};
            };

            std::vector<RenderKey> mKeys;

            // CPU-batch вершин: 1 спрайт = 2 треугольника = 6 вершин.
            // Важно: вектор живёт как scratch-buffer (reserve один раз, clear каждый кадр).
            std::vector<sf::Vertex> mVertices;

            struct TextureCacheEntry {
                core::resources::ids::TextureID id{};
                const sf::Texture* texture = nullptr;
                sf::IntRect fullRect{};
            };
            
            // Per-frame cache: TextureID -> (sf::Texture*, fullRect)
            // Линейный поиск намеренно: ожидаемо десятки уникальных текстур, overhead минимальный.
            std::vector<TextureCacheEntry> mTextureCache;

    #if !defined(NDEBUG) || defined(SFML1_PROFILE)
            FrameStats mLastStats{};
            // Scratch: уникальные sf::Texture* за кадр.
            std::vector<const sf::Texture*> mUniqueTexturePointers;
    #endif
        };

    } // namespace core::ecs