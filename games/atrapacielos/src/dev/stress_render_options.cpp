#include "pch.h"

#include "dev/stress_render_options.h"

#if defined(RETRIBUTIO_PROFILE)

#include <array>
#include <cstdlib>

namespace game::atrapacielos::dev {

    bool readRenderStressOverlayBackplateEnabled() noexcept {
#ifdef _WIN32
        std::array<char, 8> buf{};
        std::size_t required = 0;
        if (::getenv_s(&required, buf.data(), buf.size(),
                       "ATRAPACIELOS_STRESS_RENDER_OVERLAY_BACKPLATE") != 0) {
            return false;
        }
        if (required <= 1 || required > buf.size()) {
            return false;
        }
        return buf[0] != '0';
#else
        const char* s = std::getenv("ATRAPACIELOS_STRESS_RENDER_OVERLAY_BACKPLATE");
        return s != nullptr && *s != '\0' && *s != '0';
#endif
    }

} // namespace game::atrapacielos::dev

#endif