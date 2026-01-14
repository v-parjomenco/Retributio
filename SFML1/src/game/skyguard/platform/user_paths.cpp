#include "pch.h"

#include "game/skyguard/platform/user_paths.h"

#include <cstdlib>
#include <optional>
#include <string>

#ifdef _WIN32
    #include "core/compiler/platform/windows.h"
    #include <ShlObj.h> // SHGetFolderPathW, CSIDL_LOCAL_APPDATA
    #include <stdlib.h> // ::_dupenv_s
    #include <memory>   // std::unique_ptr
#else
    #include <filesystem>
#endif

namespace {

    namespace fs = std::filesystem;

    static constexpr std::string_view kSettingsFileName = "user_settings.json";

    [[nodiscard]] std::optional<fs::path> getEnvPath(const char* name) noexcept {
#ifdef _WIN32
        // MSVC: getenv() deprecated (C4996). Используем _dupenv_s и освобождаем память.
        char* raw = nullptr;
        std::size_t len = 0;

        if (::_dupenv_s(&raw, &len, name) != 0 || raw == nullptr || len == 0) {
            return std::nullopt;
        }

        // RAII: гарантируем free() даже при ранних return.
        std::unique_ptr<char, decltype(&std::free)> owned(raw, &std::free);

        if (owned.get()[0] == '\0') {
            return std::nullopt;
        }

        return fs::path(owned.get());
#else
        const char* v = std::getenv(name);
        if (v == nullptr || v[0] == '\0') {
            return std::nullopt;
        }
        return fs::path(v);
#endif
    }

#ifdef _WIN32
    [[nodiscard]] std::optional<fs::path> tryGetLocalAppData() noexcept {
        std::wstring buffer(MAX_PATH, L'\0');

        if (SUCCEEDED(SHGetFolderPathW(nullptr,
                                       CSIDL_LOCAL_APPDATA,
                                       nullptr,
                                       SHGFP_TYPE_CURRENT,
                                       buffer.data()))) {
            // SHGetFolderPathW пишет null-terminated строку.
            const std::wstring::size_type end = buffer.find(L'\0');
            if (end != std::wstring::npos) {
                buffer.resize(end);
            }
            if (!buffer.empty()) {
                return fs::path(buffer);
            }
        }

        return std::nullopt;
    }
#endif

} // namespace

namespace game::skyguard::platform {

    fs::path getUserConfigDir(const std::string_view appId) {
        // validate-on-write: appId валидируется при загрузке AppConfig (AppConfigLoader).
        // Здесь считаем, что appId "trust-on-read".
#ifdef _WIN32
        if (const auto base = tryGetLocalAppData()) {
            return (*base) / std::string(appId);
        }

        if (const auto env = getEnvPath("LOCALAPPDATA")) {
            return (*env) / std::string(appId);
        }

        return fs::temp_directory_path() / std::string(appId);

#elif defined(__APPLE__)
        if (const auto home = getEnvPath("HOME")) {
            return (*home) / "Library" / "Application Support" / std::string(appId);
        }
        return fs::temp_directory_path() / std::string(appId);

#else
        if (const auto xdg = getEnvPath("XDG_CONFIG_HOME")) {
            return (*xdg) / std::string(appId);
        }

        if (const auto home = getEnvPath("HOME")) {
            return (*home) / ".config" / std::string(appId);
        }

        return fs::temp_directory_path() / std::string(appId);
#endif
    }

    fs::path getUserSettingsPath(const std::string_view appId) {
        return getUserConfigDir(appId) / kSettingsFileName;
    }

} // namespace game::skyguard::platform