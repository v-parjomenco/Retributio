#include "pch.h"

#include "core/runtime/entry/working_directory.h"

#include <algorithm>
#include <filesystem>

#ifdef _WIN32
    #include <windows.h>
#elif defined(__linux__)
    #include <unistd.h>
#elif defined(__APPLE__)
    #include <cstdlib>
    #include <mach-o/dyld.h>
#else
    #error "Platform not supported: implement getExecutablePath() for this OS."
#endif

namespace core::runtime::entry {

namespace {

    namespace fs = std::filesystem;

    // Размер буфера для пути к исполняемому файлу.
    // PATH_MAX на Linux и macOS обычно 4096; используем явную константу для ясности.
    constexpr std::size_t kExePathBufSize = 4096;

    // Возвращает абсолютный путь к текущему исполняемому файлу через платформенный API.
    // Возвращает пустой fs::path при любой ошибке — caller проверяет empty().
    // static: внутренняя связь, не экспортируется за пределы TU.
    fs::path getExecutablePath() noexcept {
        try {
#ifdef _WIN32
            // MAX_PATH (260) покрывает подавляющее большинство реальных путей.
            // Для длинных путей с \\?\ потребуется расширенная версия — отдельный тикет.
            wchar_t buf[MAX_PATH]{};
            const DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);

            if (len == 0 || len >= MAX_PATH) {
                // len == 0: ошибка API. len >= MAX_PATH: путь обрезан — не доверяем.
                return {};
            }
            return fs::path{buf, buf + len};

#elif defined(__linux__)
            char buf[kExePathBufSize]{};
            const ssize_t len =
                ::readlink("/proc/self/exe", buf, kExePathBufSize - 1);

            if (len <= 0) {
                return {};
            }
            // readlink не добавляет null-терминатор — ставим вручную.
            buf[static_cast<std::size_t>(len)] = '\0';
            return fs::path{buf};

#elif defined(__APPLE__)
            // Первый вызов: зондируем нужный размер буфера.
            uint32_t size = static_cast<uint32_t>(kExePathBufSize);
            char buf[kExePathBufSize]{};

            if (_NSGetExecutablePath(buf, &size) != 0) {
                // Буфер оказался мал (маловероятно при kExePathBufSize = 4096).
                return {};
            }

            // Канонизируем путь: раскрываем симлинки и убираем . / ..
            char resolved[kExePathBufSize]{};
            if (::realpath(buf, resolved) == nullptr) {
                return {};
            }
            return fs::path{resolved};
#endif
        } catch (...) {
            // fs::path конструктор теоретически может бросить — поглощаем.
            return {};
        }
    }

} // namespace

    bool setWorkingDirectoryFromExecutable(
        std::span<const std::string_view> sentinels) noexcept {
        try {
            if (sentinels.empty()) {
                return false;
            }

            const fs::path exePath = getExecutablePath();
            if (exePath.empty()) {
                return false;
            }

            fs::path probe = exePath.parent_path();
            const fs::path root = probe.root_path();

            while (!probe.empty()) {
                std::error_code ec;
                // Кандидат принимается только если ВСЕ sentinel-файлы присутствуют в нём.
                const bool allFound = std::all_of(
                    sentinels.begin(),
                    sentinels.end(),
                    [&](std::string_view sentinel) {
                        ec = {};
                        return fs::exists(probe / sentinel, ec) && !ec;
                    });

                if (allFound) {
                    ec = {};
                    fs::current_path(probe, ec);
                    return !ec;
                }

                if (probe == root) {
                    // Дошли до корня файловой системы — sentinel не найден.
                    break;
                }

                probe = probe.parent_path();
            }
        } catch (...) {
            // Поглощаем: функция noexcept, caller обработает false.
        }

        return false;
    }

} // namespace core::runtime::entry