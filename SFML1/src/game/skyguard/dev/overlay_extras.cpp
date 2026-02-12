#include "pch.h"

#include "game/skyguard/dev/overlay_extras.h"

#if !defined(NDEBUG) || defined(SFML1_PROFILE)

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <string_view>

#include <SFML/Graphics/View.hpp>
#include <SFML/System/Vector2.hpp>

#include "core/ecs/components/spatial_dirty_tag.h"
#include "core/ecs/components/spatial_id_component.h"
#include "core/ecs/components/spatial_streamed_out_tag.h"
#include "core/ecs/components/sprite_component.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/render/sprite_bounds.h"
#include "core/ecs/systems/debug_overlay_system.h"
#include "core/ecs/systems/spatial_index_system.h"
#include "core/ecs/world.h"
#include "core/spatial/aabb2.h"
#include "core/spatial/chunk_coord.h"
#include "core/utils/math_constants.h"

#include "game/skyguard/ecs/queries/local_player_query.h"
#include "game/skyguard/presentation/background_renderer.h"
#include "game/skyguard/presentation/view_manager.h"
#include "game/skyguard/utils/debug_format.h"

#if defined(SFML1_PROFILE)
#include "game/skyguard/dev/stress_runtime_stamp.h"
#endif

namespace game::skyguard::dev {

    void populateDebugOverlayExtraLines(
        core::ecs::DebugOverlaySystem& overlay,
        core::ecs::World& world,
        const core::ecs::SpatialIndexSystem* spatialIndex,
        const presentation::BackgroundRenderer& background,
        const presentation::ViewManager& viewManager
#if defined(SFML1_PROFILE)
        , const StressRuntimeStamp* stressStamp
#endif
    ) {
        overlay.clearExtraText();

        // ========================================================================================
        // Background (Debug + Profile)
        // ========================================================================================
        {
            std::array<char, 256> buf{};
            const std::size_t len = utils::formatBackgroundStatsLine(
                buf.data(), buf.size(), background);

            // Если обрезали строку — увеличь буфер или сократи формат.
            assert(len < buf.size() &&
                   "overlay_extras: background debug line truncated. Increase buffer.");

            if (len > 0) {
                overlay.appendExtraLine(std::string_view(buf.data(), len));
            }
        }

        // ========================================================================================
        // Camera (Debug only, не Profile)
        // ========================================================================================
#if !defined(NDEBUG)
        {
            core::ecs::Entity e{};
            const core::ecs::TransformComponent* tr = nullptr;

            if (ecs::queries::tryGetLocalPlayerTransform(world, e, tr)) {
                std::array<char, 256> buf{};
                const auto& vw = viewManager.getWorldView();
                const sf::Vector2f off = viewManager.getCameraOffset();

                const std::size_t len = utils::formatCameraStatsLine(
                    buf.data(), buf.size(), tr->position.y, vw.getCenter().y,
                    vw.getSize().y, off.y, viewManager.getCameraCenterYMax());

                if (len > 0) {
                    overlay.appendExtraLine(std::string_view(buf.data(), len));
                }
            }
        }
#endif

        // ========================================================================================
        // Streaming stats + Cell health (Debug + Profile, если spatial index привязан)
        // ========================================================================================
        if (spatialIndex) {
            const auto& idx = spatialIndex->index();
            const auto& worldView = viewManager.getWorldView();
            const std::int32_t chunkSize = idx.chunkSizeWorld();

            // View + winOrigin.
            {
                const auto origin = idx.windowOrigin();
                std::array<char, 256> buf{};
                const std::size_t len = utils::formatStreamingStatsLine(
                    buf.data(), buf.size(), worldView, origin, chunkSize);
                if (len > 0) {
                    overlay.appendExtraLine(std::string_view(buf.data(), len));
                }
            }

            // CellsHealth для чанка, в котором центр камеры.
            {
                const sf::Vector2f viewCenter = worldView.getCenter();
                const core::spatial::ChunkCoord viewChunk = core::spatial::worldToChunk(
                    core::spatial::WorldPosf{viewCenter.x, viewCenter.y}, chunkSize);
                const auto health = idx.debugCellHealthForChunk(viewChunk);

                std::array<char, 256> buf{};
                const std::size_t len = utils::formatCellHealthLine(
                    buf.data(), buf.size(),
                    health.maxCellLen, health.sumCellLen, health.dupApprox, health.loaded);
                if (len > 0) {
                    overlay.appendExtraLine(std::string_view(buf.data(), len));
                }
            }
        }

        // ========================================================================================
        // Stress runtime stamp (Profile only)
        // ========================================================================================
#if defined(SFML1_PROFILE)
        if (stressStamp) {
            std::array<char, 256> buf{};
            const std::size_t len = utils::formatStressStampLine(
                buf.data(), buf.size(),
                stressStamp->mode, stressStamp->seed,
                stressStamp->entitiesPerChunk, stressStamp->texCount,
                stressStamp->zLayers,
                stressStamp->windowWidth, stressStamp->windowHeight);
            if (len > 0) {
                overlay.appendExtraLine(std::string_view(buf.data(), len));
            }
        }
#endif

        // ========================================================================================
        // Player watch: state + visibility prediction (Debug + Profile)
        // ========================================================================================
        {
            core::ecs::Entity e{};
            const core::ecs::TransformComponent* tr = nullptr;

            if (!ecs::queries::tryGetLocalPlayerTransform(world, e, tr)) {
                return;
            }

            // --- Данные для обеих строк (state + vis) ---

            const auto* spatialId =
                world.tryGetComponent<core::ecs::SpatialIdComponent>(e);
            const bool hasSpatialId = (spatialId != nullptr);
            const std::uint32_t spatialIdVal =
                hasSpatialId ? static_cast<std::uint32_t>(spatialId->id) : 0u;
            const bool hasDirty = world.hasTag<core::ecs::SpatialDirtyTag>(e);
            const bool hasStreamedOut =
                world.hasTag<core::ecs::SpatialStreamedOutTag>(e);

            // Player: entity, spatialId, position, dirty, streamedOut.
            {
                std::array<char, 256> buf{};
                const std::size_t len = utils::formatPlayerWatchStateLine(
                    buf.data(), buf.size(), core::ecs::toUint(e),
                    hasSpatialId, spatialIdVal, hasDirty, hasStreamedOut, tr->position);
                if (len > 0) {
                    overlay.appendExtraLine(std::string_view(buf.data(), len));
                }
            }

            // PlayerVis: fineCullPass, inQuery, predicted, last/new AABB.
            {
                const core::spatial::Aabb2 lastAabb =
                    hasSpatialId ? spatialId->lastAabb : core::spatial::Aabb2{};
                const auto* sprite =
                    world.tryGetComponent<core::ecs::SpriteComponent>(e);

                core::spatial::Aabb2 newAabb = lastAabb;
                if (sprite != nullptr) {
                    const float rotationDegrees = tr->rotationDegrees;
                    if (rotationDegrees == 0.f) {
                        newAabb = core::ecs::render::computeSpriteAabbNoRotation(
                            tr->position, sprite->origin, sprite->scale, sprite->textureRect);
                    } else {
                        const float radians = rotationDegrees * core::utils::kDegToRad;
                        const float cachedSin = std::sin(radians);
                        const float cachedCos = std::cos(radians);
                        newAabb = core::ecs::render::computeSpriteAabbRotated(
                            tr->position, sprite->origin, sprite->scale, sprite->textureRect,
                            cachedSin, cachedCos);
                    }
                }

                // viewAabb из камеры (для fineCullPass).
                const sf::View& worldView = viewManager.getWorldView();
                const sf::Vector2f viewSize = worldView.getSize();
                const sf::Vector2f viewCenter = worldView.getCenter();
                const sf::Vector2f topLeft{viewCenter.x - viewSize.x * 0.5f,
                                           viewCenter.y - viewSize.y * 0.5f};
                const core::spatial::Aabb2 viewAabb{
                    topLeft.x, topLeft.y,
                    topLeft.x + viewSize.x, topLeft.y + viewSize.y};

                const bool fineCullPass = hasSpatialId
                    ? core::spatial::intersectsInclusive(lastAabb, viewAabb)
                    : false;

                const bool inQuery = (hasSpatialId && spatialIndex)
                    ? spatialIndex->index().debugWasInLastQuery(spatialId->id)
                    : false;

                // predictedVisible != "реально отрисовано": прогноз по условиям UI-pass.
                const bool predictedVisible =
                    (sprite != nullptr) && inQuery && fineCullPass && !hasStreamedOut;

                std::array<char, 256> buf{};
                const std::size_t len = utils::formatPlayerWatchVisibilityLine(
                    buf.data(), buf.size(), lastAabb, newAabb, fineCullPass,
                    inQuery, predictedVisible);
                if (len > 0) {
                    overlay.appendExtraLine(std::string_view(buf.data(), len));
                }
            }
        }
    }

} // namespace game::skyguard::dev

#endif // !defined(NDEBUG) || defined(SFML1_PROFILE)