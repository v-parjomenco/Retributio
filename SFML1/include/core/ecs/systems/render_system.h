// ================================================================================================
// File: core/ecs/systems/render_system.h
// Purpose: Production render system with ID-based sprites, deterministic ordering, batching-ready.
// Used by: World/SystemManager
// Related headers: sprite_component.h, transform_component.h, resource_manager.h
// ================================================================================================
#pragma once

#include <cstdint>
#include <vector>

#include <SFML/Graphics.hpp>

#include "core/ecs/entity.h"
#include "core/ecs/system.h"
#include "core/resources/ids/resource_ids.h"
#include "core/resources/resource_manager.h"

namespace core::ecs {

    class RenderSystem final : public ISystem {
      public:
        explicit RenderSystem(core::resources::ResourceManager& resources)
            : mResources(&resources) {
        }

        void render(World& world, sf::RenderWindow& window) override;

        struct FrameStats {
            std::size_t  spriteCount      = 0;
            std::size_t  vertexCount      = 0;
            std::size_t  batchDrawCalls   = 0;
            std::size_t  textureSwitches  = 0;
            std::uint64_t cpuTotalUs      = 0;
            std::uint64_t cpuDrawUs       = 0;
        };

        [[nodiscard]] FrameStats getLastFrameStats() const noexcept;

      private:
        core::resources::ResourceManager* mResources;

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

#ifndef NDEBUG
        FrameStats mLastStats{};
#endif

    };

} // namespace core::ecs