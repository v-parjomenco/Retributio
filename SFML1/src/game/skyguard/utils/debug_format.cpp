#include "pch.h"

#include "game/skyguard/utils/debug_format.h"

#if !defined(NDEBUG) || defined(SFML1_PROFILE)

#include <SFML/Graphics/Rect.hpp>

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

        if (!detail::appendLiteral(it, end, "Background: tiles ")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, stats.tilesCovered)) {
            return static_cast<std::size_t>(it - buf);
        }

        if (!detail::appendLiteral(it, end, "  draws ")) {
            return static_cast<std::size_t>(it - buf);
        }
        if (!detail::appendNumber(it, end, stats.drawCalls)) {
            return static_cast<std::size_t>(it - buf);
        }

        if (!detail::appendLiteral(it, end, "  tile ")) {
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

        if (!detail::appendLiteral(it, end, "  view ")) {
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

        if (!detail::appendLiteral(it, end, "  pos ")) {
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

} // namespace game::skyguard::utils

#endif // !defined(NDEBUG) || defined(SFML1_PROFILE)
