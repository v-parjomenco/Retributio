// ================================================================================================
// File: game/skyguard/streaming/chunk_content_provider.h
// Purpose: Game-layer chunk content provider for streaming (active-set ECS only)
// Notes:
//  - ECS only stores the active runtime set (visual/operational layer).
//  - The truth of the world should live in the chunk DB/LOD, not in ECS.
//  - Provider MUST NOT allocate in fillChunkContent(); caller owns the buffer.
// ================================================================================================
#pragma once

#include <cstddef>
#include <span>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include "core/resources/keys/resource_key.h"
#include "core/spatial/chunk_coord.h"

namespace game::skyguard::streaming {

    struct ChunkEntityDesc final {
        core::spatial::WorldPosf localPos{};
        core::resources::TextureKey texture{};
        sf::IntRect textureRect{};
        sf::Vector2f scale{1.f, 1.f};
        sf::Vector2f origin{0.f, 0.f};
        float zOrder{0.f};
    };

    class IChunkContentProvider {
      public:
        virtual ~IChunkContentProvider() = default;

        // Максимальное количество сущностей, которое этот провайдер может сгенерировать
        // для любого отдельного чанка.
        // КОНТРАКТ:
        //  - Вызывается в cold-path (bind/init).
        //  - Вызывающая сторона выделяет буфер этого размера.
        //  - fillChunkContent() НЕ ДОЛЖЕН превышать это значение.
        [[nodiscard]] virtual std::size_t maxEntitiesPerChunk() const noexcept = 0;

        // Заполняет предоставленный вызывающей стороной буфер содержимым чанка.
        // @return Количество записанных элементов в out[0..count).
        // КОНТРАКТ:
        //  - out.size() >= maxEntitiesPerChunk() гарантируется вызывающей стороной.
        //  - Должен быть детерминированным для одинаковых (coord + внутренний seed/конфиг).
        //  - Не должен аллоцировать и не должен бросать исключения (в hot/update путях).
        [[nodiscard]] virtual std::size_t fillChunkContent(
            core::spatial::ChunkCoord coord,
            std::span<ChunkEntityDesc> out) = 0;

        // Optional notification for provider-side bookkeeping.
        virtual void onChunkUnloaded(core::spatial::ChunkCoord coord) noexcept {
            (void) coord;
        }
    };

    class EmptyChunkContentProvider final : public IChunkContentProvider {
      public:
        [[nodiscard]] std::size_t maxEntitiesPerChunk() const noexcept override {
            return 0u;
        }

        [[nodiscard]] std::size_t fillChunkContent(
            core::spatial::ChunkCoord,
            std::span<ChunkEntityDesc>) override {
            return 0u;
        }
    };

} // namespace game::skyguard::streaming
