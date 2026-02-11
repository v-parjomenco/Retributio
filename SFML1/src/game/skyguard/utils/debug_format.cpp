#include "pch.h"

#include "game/skyguard/utils/debug_format.h"

#if !defined(NDEBUG) || defined(SFML1_PROFILE)

#include <SFML/Graphics/Rect.hpp>

#include "core/spatial/chunk_coord.h"
#include "game/skyguard/presentation/background_renderer.h"

namespace game::skyguard::utils {

    std::size_t formatBackgroundStatsLine(
        char* buf,
        std::size_t cap,
        const presentation::BackgroundRenderer& backgroundRenderer) noexcept {
        if (cap == 0) {
            return 0;
        }

        const auto& stats = backgroundRenderer.getLastFrameStats();

        const std::uint32_t viewW = static_cast<std::uint32_t>(stats.visibleRect.size.x);
        const std::uint32_t viewH = static_cast<std::uint32_t>(stats.visibleRect.size.y);
        const std::int32_t posX = static_cast<std::int32_t>(stats.visibleRect.position.x);
        const std::int32_t posY = static_cast<std::int32_t>(stats.visibleRect.position.y);

        char* it = buf;
        char* end = buf + cap;

        if (!detail::appendLiteral(it, end, "Background: tiles=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, stats.tilesCovered)) {
            return static_cast<std::size_t>(it - buf);
        }

        if (!detail::appendLiteral(it, end, " draws=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, stats.drawCalls)) {
            return static_cast<std::size_t>(it - buf);
        }

        if (!detail::appendLiteral(it, end, " tile=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, stats.tileSize.x)) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, "x")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, stats.tileSize.y)) {
            return static_cast<std::size_t>(it - buf);
        }

        if (!detail::appendLiteral(it, end, " view=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, viewW)) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, "x")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, viewH)) {
            return static_cast<std::size_t>(it - buf);
        }

        if (!detail::appendLiteral(it, end, " pos=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, posX)) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, ",")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, posY)) {
            return static_cast<std::size_t>(it - buf);
        }

        return static_cast<std::size_t>(it - buf);
    }

    std::size_t formatStreamingStatsLine(char* buf, std::size_t cap,
                                         const sf::View& view,
                                         const core::spatial::ChunkCoord windowOrigin,
                                         const std::int32_t chunkSizeWorld) noexcept {
        if (cap == 0) {
            return 0;
        }

        const sf::Vector2f center = view.getCenter();
        const sf::Vector2f size = view.getSize();

        const core::spatial::ChunkCoord viewChunk = core::spatial::worldToChunk(
            core::spatial::WorldPosf{center.x, center.y}, chunkSizeWorld);

        char* it = buf;
        char* end = buf + cap;

        if (!detail::appendLiteral(it, end, "View: centerX=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendFixed2(it, end, center.x)) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, " centerY=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendFixed2(it, end, center.y)) {
            return static_cast<std::size_t>(it - buf);
        }

        if (!detail::appendLiteral(it, end, " sizeX=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendFixed2(it, end, size.x)) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, " sizeY=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendFixed2(it, end, size.y)) {
            return static_cast<std::size_t>(it - buf);
        }

        if (!detail::appendLiteral(it, end, " viewChunk=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, viewChunk.x)) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, ",")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, viewChunk.y)) {
            return static_cast<std::size_t>(it - buf);
        }

        if (!detail::appendLiteral(it, end, " winOrigin=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, windowOrigin.x)) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, ",")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, windowOrigin.y)) {
            return static_cast<std::size_t>(it - buf);
        }

        return static_cast<std::size_t>(it - buf);
    }

    std::size_t formatCellHealthLine(char* buf, std::size_t cap,
                                     const std::uint32_t maxLen,
                                     const std::uint32_t sumLen,
                                     const std::uint32_t dupApprox,
                                     const std::uint32_t loaded) noexcept {
        if (cap == 0) {
            return 0;
        }

        char* it = buf;
        char* end = buf + cap;

        if (!detail::appendLiteral(it, end, "CellsHealth: maxLen=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, maxLen)) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, " sumLen=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, sumLen)) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, " dupApprox=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, dupApprox)) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, " loaded=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, loaded)) {
            return static_cast<std::size_t>(it - buf);
        }

        return static_cast<std::size_t>(it - buf);
    }

    std::size_t formatPlayerWatchStateLine(char* buf, std::size_t cap,
                                           const std::uint64_t entityId,
                                           const bool hasSpatialId,
                                           const std::uint32_t spatialId,
                                           const bool hasDirty,
                                           const bool hasStreamedOut,
                                           const sf::Vector2f& position) noexcept {
        if (cap == 0) {
            return 0;
        }

        char* it = buf;
        char* end = buf + cap;

        if (!detail::appendLiteral(it, end, "Player: entity=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, entityId)) {
            return static_cast<std::size_t>(it - buf);
        }

        if (!detail::appendLiteral(it, end, " spatialId=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (hasSpatialId) {
            if (!detail::appendNumber(it, end, spatialId)) {
                return static_cast<std::size_t>(it - buf);
            }
        } else {
            if (!detail::appendLiteral(it, end, "-")) {
                return static_cast<std::size_t>(it - buf);
            }
        }

        if (!detail::appendLiteral(it, end, " posX=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendFixed1(it, end, position.x)) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, " posY=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendFixed1(it, end, position.y)) {
            return static_cast<std::size_t>(it - buf);
        }

        if (!detail::appendLiteral(it, end, " dirty=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, static_cast<std::uint32_t>(hasDirty))) {
            return static_cast<std::size_t>(it - buf);
        }

        if (!detail::appendLiteral(it, end, " streamedOut=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, static_cast<std::uint32_t>(hasStreamedOut))) {
            return static_cast<std::size_t>(it - buf);
        }

        return static_cast<std::size_t>(it - buf);
    }

    std::size_t formatPlayerWatchVisibilityLine(char* buf, std::size_t cap,
                                                const core::spatial::Aabb2& lastAabb,
                                                const core::spatial::Aabb2& newAabb,
                                                const bool fineCullPass,
                                                const bool inQuery,
                                                const bool predictedVisible) noexcept {
        if (cap == 0) {
            return 0;
        }

        char* it = buf;
        char* end = buf + cap;

        if (!detail::appendLiteral(it, end, "PlayerVis: fineCullPass=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, static_cast<std::uint32_t>(fineCullPass))) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, " inQuery=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, static_cast<std::uint32_t>(inQuery))) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, " predicted=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, static_cast<std::uint32_t>(predictedVisible))) {
            return static_cast<std::size_t>(it - buf);
        }

        if (!detail::appendLiteral(it, end, " last=(")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, static_cast<std::int32_t>(lastAabb.minX))) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, ",")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, static_cast<std::int32_t>(lastAabb.minY))) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, ")->(")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, static_cast<std::int32_t>(lastAabb.maxX))) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, ",")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, static_cast<std::int32_t>(lastAabb.maxY))) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, ")")) {
            return static_cast<std::size_t>(it - buf);
        }

        if (!detail::appendLiteral(it, end, " new=(")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, static_cast<std::int32_t>(newAabb.minX))) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, ",")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, static_cast<std::int32_t>(newAabb.minY))) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, ")->(")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, static_cast<std::int32_t>(newAabb.maxX))) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, ",")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, static_cast<std::int32_t>(newAabb.maxY))) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, ")")) {
            return static_cast<std::size_t>(it - buf);
        }

        return static_cast<std::size_t>(it - buf);
    }

    std::size_t formatDensityLine(char* buf, std::size_t cap,
                                  const float effectivePerChunk,
                                  const std::size_t configuredPerChunk,
                                  const std::int32_t chunkSizeWorld,
                                  const std::int32_t cellSizeWorld,
                                  const sf::Vector2f& viewSize) noexcept {
        if (cap == 0) {
            return 0;
        }

        char* it = buf;
        char* end = buf + cap;

        if (!detail::appendLiteral(it, end, "Density: effectivePerChunk=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendFixed1(it, end, effectivePerChunk)) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, " configuredPerChunk=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, configuredPerChunk)) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, " chunkSize=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, chunkSizeWorld)) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, " cellSize=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, cellSizeWorld)) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, " view=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendFixed1(it, end, viewSize.x)) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, "x")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendFixed1(it, end, viewSize.y)) {
            return static_cast<std::size_t>(it - buf);
        }

        return static_cast<std::size_t>(it - buf);
    }

    std::size_t formatRangeLine(char* buf, std::size_t cap,
                                const std::int32_t chunkMinX, const std::int32_t chunkMinY,
                                const std::int32_t chunkMaxX, const std::int32_t chunkMaxY,
                                const std::int32_t cellMinX, const std::int32_t cellMinY,
                                const std::int32_t cellMaxX, const std::int32_t cellMaxY) noexcept {
        if (cap == 0) {
            return 0;
        }

        char* it = buf;
        char* end = buf + cap;

        if (!detail::appendLiteral(it, end, "Range: chunkMin=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, chunkMinX)) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, ",")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, chunkMinY)) {
            return static_cast<std::size_t>(it - buf);
        }

        if (!detail::appendLiteral(it, end, " chunkMax=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, chunkMaxX)) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, ",")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, chunkMaxY)) {
            return static_cast<std::size_t>(it - buf);
        }

        if (!detail::appendLiteral(it, end, " cellMin=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, cellMinX)) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, ",")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, cellMinY)) {
            return static_cast<std::size_t>(it - buf);
        }

        if (!detail::appendLiteral(it, end, " cellMax=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, cellMaxX)) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, ",")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, cellMaxY)) {
            return static_cast<std::size_t>(it - buf);
        }

        return static_cast<std::size_t>(it - buf);
    }

    namespace {
        [[nodiscard]] char residencyShort(const core::spatial::ResidencyState st) noexcept {
            switch (st) {
            case core::spatial::ResidencyState::Loaded:
                return 'L';
            case core::spatial::ResidencyState::Loading:
                return 'O';
            case core::spatial::ResidencyState::Unloading:
                return 'U';
            case core::spatial::ResidencyState::Unloaded:
            default:
                return '-';
            }
        }
    } // namespace

    std::size_t formatResidencyLine(char* buf, std::size_t cap,
                                    const core::spatial::ChunkCoord& viewChunk,
                                    const core::spatial::ResidencyState viewState,
                                    const core::spatial::ChunkCoord& playerChunk,
                                    const core::spatial::ResidencyState playerState,
                                    const core::spatial::ChunkCoord& focusChunk,
                                    const core::spatial::ResidencyState focusState,
                                    const core::spatial::ChunkCoord& originCurrent,
                                    const core::spatial::ChunkCoord& originDesired,
                                    const std::uint32_t initialLoadRemaining,
                                    const std::uint32_t loadsThisFrame,
                                    const std::uint32_t unloadsThisFrame) noexcept {
        if (cap == 0) {
            return 0;
        }

        char* it = buf;
        char* end = buf + cap;

        if (!detail::appendLiteral(it, end, "Residency: V=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (it >= end) {
            return static_cast<std::size_t>(it - buf);
        }
        *it++ = residencyShort(viewState);

        if (!detail::appendLiteral(it, end, " P=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (it >= end) {
            return static_cast<std::size_t>(it - buf);
        }
        *it++ = residencyShort(playerState);

        if (!detail::appendLiteral(it, end, " F=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (it >= end) {
            return static_cast<std::size_t>(it - buf);
        }
        *it++ = residencyShort(focusState);

        if (!detail::appendLiteral(it, end, " view=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, viewChunk.x)) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, ",")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, viewChunk.y)) {
            return static_cast<std::size_t>(it - buf);
        }

        if (!detail::appendLiteral(it, end, " player=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, playerChunk.x)) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, ",")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, playerChunk.y)) {
            return static_cast<std::size_t>(it - buf);
        }

        if (!detail::appendLiteral(it, end, " focus=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, focusChunk.x)) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, ",")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, focusChunk.y)) {
            return static_cast<std::size_t>(it - buf);
        }

        if (!detail::appendLiteral(it, end, " origin=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, originCurrent.x)) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, ",")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, originCurrent.y)) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, "->")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, originDesired.x)) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendLiteral(it, end, ",")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, originDesired.y)) {
            return static_cast<std::size_t>(it - buf);
        }

        if (!detail::appendLiteral(it, end, " initialLoadRemaining=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, initialLoadRemaining)) {
            return static_cast<std::size_t>(it - buf);
        }

        if (!detail::appendLiteral(it, end, " loadsThisFrame=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, loadsThisFrame)) {
            return static_cast<std::size_t>(it - buf);
        }

        if (!detail::appendLiteral(it, end, " unloadsThisFrame=")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, unloadsThisFrame)) {
            return static_cast<std::size_t>(it - buf);
        }

        return static_cast<std::size_t>(it - buf);
    }

} // namespace game::skyguard::utils

#endif // !defined(NDEBUG) || defined(SFML1_PROFILE)
