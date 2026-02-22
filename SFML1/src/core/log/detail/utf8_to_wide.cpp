#include "pch.h"

#include "core/log/detail/utf8_to_wide.h"

#ifdef _WIN32
    #include <windows.h>
#endif

namespace core::log::detail {

#ifdef _WIN32

    std::wstring utf8ToWide(std::string_view text) {
        if (text.empty()) {
            return {};
        }

        const int required = MultiByteToWideChar(
            CP_UTF8,
            0,
            text.data(),
            static_cast<int>(text.size()),
            nullptr,
            0);

        if (required <= 0) {
            return L"<UTF-8 to UTF-16 conversion failed>";
        }

        std::wstring result;
        result.resize(static_cast<std::size_t>(required));

        const int converted = MultiByteToWideChar(
            CP_UTF8,
            0,
            text.data(),
            static_cast<int>(text.size()),
            result.data(),
            required);

        if (converted <= 0) {
            return L"<UTF-8 to UTF-16 conversion failed>";
        }

        return result;
    }

#endif // _WIN32

} // namespace core::log::detail