#include "pch.h"

#include "core/runtime/entry/working_directory.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <string_view>

#ifdef _WIN32
    #include <windows.h>
#elif defined(__linux__)
    #include <sys/stat.h>
    #include <unistd.h>
#elif defined(__APPLE__)
    #include <mach-o/dyld.h>
    #include <sys/stat.h>
    #include <unistd.h>
#else
    #error "Platform not supported: implement getExecutablePath() for this OS."
#endif

namespace core::runtime::entry {

namespace {

    constexpr std::size_t kPathInitial = 512;
    constexpr std::size_t kPathHardCap = 32768;

    // Нужен запас capacity, чтобы строить "dir + '/' + sentinel" без новых аллокаций.
    //
    // Инварианты:
    //  - kJoinSlack >= 1 + max_sentinel_len (1 = separator).
    //  - После стадии "exe-path" probing не должен менять capacity() (0 heap alloc в цикле).
    //    Это проверяется через assert в setWorkingDirectoryFromExecutable и existsFile*().
    constexpr std::size_t kJoinSlack = 64;

#ifdef _WIN32

    constexpr wchar_t kSep = L'\\';

    bool isSep(wchar_t c) noexcept {
        return c == L'\\' || c == L'/';
    }

    bool isDriveRoot(const std::wstring& p) noexcept {
        return p.size() == 3 && p[1] == L':' && isSep(p[2]);
    }

    std::size_t uncRootLen(const std::wstring& p) noexcept {
        if (p.size() < 5) {
            return 0;
        }
        if (!(p[0] == L'\\' && p[1] == L'\\')) {
            return 0;
        }

        std::size_t i = 2;
        while (i < p.size() && !isSep(p[i])) {
            ++i;
        }
        if (i == 2 || i >= p.size()) {
            return 0;
        }

        while (i < p.size() && isSep(p[i])) {
            ++i;
        }
        const std::size_t shareStart = i;
        if (shareStart >= p.size()) {
            return 0;
        }

        while (i < p.size() && !isSep(p[i])) {
            ++i;
        }
        if (i == shareStart) {
            return 0;
        }

        return i;
    }

    void trimTrailingSeps(std::wstring& p) noexcept {
        while (p.size() > 1 && isSep(p.back())) {
            if (isDriveRoot(p)) {
                return;
            }
            const std::size_t uncLen = uncRootLen(p);
            if (uncLen != 0 && p.size() == uncLen + 1) {
                return;
            }
            p.pop_back();
        }
    }

    bool popParent(std::wstring& dir) noexcept {
        trimTrailingSeps(dir);
        if (dir.empty() || isDriveRoot(dir)) {
            return false;
        }

        const std::size_t uncLen = uncRootLen(dir);
        if (uncLen != 0 && dir.size() <= uncLen) {
            return false;
        }

        std::size_t i = dir.size();
        while (i > 0 && !isSep(dir[i - 1])) {
            --i;
        }
        if (i == 0) {
            return false;
        }

        while (i > 0 && isSep(dir[i - 1])) {
            --i;
        }

        if (uncLen != 0) {
            if (i <= uncLen) {
                dir.resize(uncLen);
                if (dir.empty() || !isSep(dir.back())) {
                    dir.push_back(kSep);
                }
                return true;
            }
        }

        if (i == 2 && dir.size() >= 2 && dir[1] == L':') {
            dir.resize(3);
            dir[2] = kSep;
            return true;
        }

        if (i == 0) {
            return false;
        }

        dir.resize(i);
        trimTrailingSeps(dir);

        if (dir.size() == 2 && dir[1] == L':') {
            dir.push_back(kSep);
        }

        return true;
    }

    bool ensureJoinSlack(std::wstring& buf) noexcept {
        if (buf.capacity() >= buf.size() + kJoinSlack) {
            return true;
        }

        // Аллокация допускается только как часть "exe-path" буфера:
        // буфер используется как scratch для join без дальнейших аллокаций.
        const std::size_t target = buf.size() + kJoinSlack;
        if (target > kPathHardCap + kJoinSlack) {
            return false;
        }
        try {
            buf.reserve(target);
            return true;
        } catch (...) {
            return false;
        }
    }

    std::wstring getExecutablePathW() noexcept {
        try {
            std::wstring buf(kPathInitial, L'\0');
            while (buf.size() <= kPathHardCap) {
                const DWORD len =
                    GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
                if (len == 0) {
                    return {};
                }

                if (static_cast<std::size_t>(len) < buf.size()) {
                    buf.resize(static_cast<std::size_t>(len));
                    if (!ensureJoinSlack(buf)) {
                        return {};
                    }
                    return buf;
                }

                if (buf.size() == kPathHardCap) {
                    return {};
                }

                const std::size_t next = buf.size() * 2;
                if (next <= buf.size()) {
                    return {};
                }
                const std::size_t capped = (next < kPathHardCap) ? next : kPathHardCap;
                buf.resize(capped, L'\0');
            }
        } catch (...) {
        }

        return {};
    }

    void stripToParentDir(std::wstring& p) noexcept {
        trimTrailingSeps(p);

        std::size_t i = p.size();
        while (i > 0 && !isSep(p[i - 1])) {
            --i;
        }
        if (i == 0) {
            p.clear();
            return;
        }

        while (i > 0 && isSep(p[i - 1])) {
            --i;
        }

        if (i == 2 && p.size() >= 2 && p[1] == L':') {
            p.resize(3);
            p[2] = kSep;
            return;
        }

        const std::size_t uncLen = uncRootLen(p);
        if (uncLen != 0 && i <= uncLen) {
            p.resize(uncLen);
            if (p.empty() || !isSep(p.back())) {
                p.push_back(kSep);
            }
            return;
        }

        p.resize(i);
        trimTrailingSeps(p);
    }

    bool existsFileW(std::wstring& dirScratch, std::string_view sentinel) noexcept {
        assert(!sentinel.empty());
        assert(sentinel.front() != '/');
        assert(sentinel.front() != '\\');

        const std::size_t baseLen = dirScratch.size();
        [[maybe_unused]] const std::size_t capBefore = dirScratch.capacity();

        if (!dirScratch.empty() && !isSep(dirScratch.back())) {
            dirScratch.push_back(kSep);
        }

        for (char c : sentinel) {
            const wchar_t wc = (c == '/') ? L'\\' : static_cast<wchar_t>(c);
            dirScratch.push_back(wc);
        }

        assert(dirScratch.capacity() == capBefore);

        const DWORD attrs = GetFileAttributesW(dirScratch.c_str());

        dirScratch.resize(baseLen);
        assert(dirScratch.capacity() == capBefore);

        if (attrs == INVALID_FILE_ATTRIBUTES) {
            return false;
        }
        return (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    bool setCwdW(const std::wstring& dir) noexcept {
        if (dir.empty()) {
            return false;
        }
        return SetCurrentDirectoryW(dir.c_str()) != 0;
    }

#else

    constexpr char kSep = '/';

    bool ensureJoinSlack(std::string& buf) noexcept {
        if (buf.capacity() >= buf.size() + kJoinSlack) {
            return true;
        }

        const std::size_t target = buf.size() + kJoinSlack;
        if (target > kPathHardCap + kJoinSlack) {
            return false;
        }
        try {
            buf.reserve(target);
            return true;
        } catch (...) {
            return false;
        }
    }

    std::string getExecutablePathNarrow() noexcept {
        try {
#if defined(__linux__)
            std::string buf(kPathInitial, '\0');
            while (buf.size() <= kPathHardCap) {
                const ssize_t len = ::readlink("/proc/self/exe", buf.data(), buf.size() - 1);
                if (len <= 0) {
                    return {};
                }

                const std::size_t ulen = static_cast<std::size_t>(len);
                if (ulen < buf.size() - 1) {
                    buf.resize(ulen);
                    if (!ensureJoinSlack(buf)) {
                        return {};
                    }
                    return buf;
                }

                if (buf.size() == kPathHardCap) {
                    return {};
                }

                const std::size_t next = buf.size() * 2;
                if (next <= buf.size()) {
                    return {};
                }
                const std::size_t capped = (next < kPathHardCap) ? next : kPathHardCap;
                buf.resize(capped, '\0');
            }
            return {};
#elif defined(__APPLE__)
            uint32_t size = 0;
            if (_NSGetExecutablePath(nullptr, &size) != -1) {
                return {};
            }
            if (size == 0u || static_cast<std::size_t>(size) > kPathHardCap) {
                return {};
            }

            std::unique_ptr<char[]> buf{};
            for (int attempt = 0; attempt < 2; ++attempt) {
                buf.reset(new (std::nothrow) char[size]);
                if (!buf) {
                    return {};
                }

                uint32_t outSize = size;
                if (_NSGetExecutablePath(buf.get(), &outSize) == 0) {
                    break;
                }

                if (outSize <= size || static_cast<std::size_t>(outSize) > kPathHardCap) {
                    return {};
                }
                size = outSize;
            }
            if (!buf) {
                return {};
            }

            using FreeDeleter = decltype(&std::free);
            std::unique_ptr<char, FreeDeleter> resolved{
                ::realpath(buf.get(), nullptr),
                &std::free,
            };
            if (resolved) {
                std::string out{resolved.get()};
                if (!ensureJoinSlack(out)) {
                    return {};
                }
                return out;
            }

            std::string out{buf.get()};
            if (!ensureJoinSlack(out)) {
                return {};
            }
            return out;
#else
            return {};
#endif
        } catch (...) {
            return {};
        }
    }

    void trimTrailingSeps(std::string& p) noexcept {
        while (p.size() > 1 && p.back() == '/') {
            p.pop_back();
        }
    }

    void stripToParentDir(std::string& p) noexcept {
        trimTrailingSeps(p);

        std::size_t i = p.size();
        while (i > 0 && p[i - 1] != '/') {
            --i;
        }
        if (i == 0) {
            p.clear();
            return;
        }

        while (i > 1 && p[i - 1] == '/') {
            --i;
        }

        if (i == 0) {
            p = "/";
            return;
        }

        p.resize(i);
        trimTrailingSeps(p);
        if (p.empty()) {
            p = "/";
        }
    }

    bool popParent(std::string& dir) noexcept {
        trimTrailingSeps(dir);
        if (dir.empty() || dir == "/") {
            return false;
        }

        std::size_t i = dir.size();
        while (i > 0 && dir[i - 1] != '/') {
            --i;
        }
        if (i == 0) {
            dir = "/";
            return true;
        }

        while (i > 1 && dir[i - 1] == '/') {
            --i;
        }

        dir.resize(i);
        trimTrailingSeps(dir);
        if (dir.empty()) {
            dir = "/";
        }
        return true;
    }

    bool existsFileNarrow(std::string& dirScratch, std::string_view sentinel) noexcept {
        assert(!sentinel.empty());
        assert(sentinel.front() != '/');
        assert(sentinel.front() != '\\');

        const std::size_t baseLen = dirScratch.size();
        [[maybe_unused]] const std::size_t capBefore = dirScratch.capacity();

        if (!dirScratch.empty() && dirScratch.back() != '/') {
            dirScratch.push_back('/');
        }

        dirScratch.append(sentinel.data(), sentinel.size());

        assert(dirScratch.capacity() == capBefore);

        struct stat st {};
        const int rc = ::stat(dirScratch.c_str(), &st);

        dirScratch.resize(baseLen);
        assert(dirScratch.capacity() == capBefore);

        if (rc != 0) {
            return false;
        }
        return !S_ISDIR(st.st_mode);
    }

    bool setCwdNarrow(const std::string& dir) noexcept {
        if (dir.empty()) {
            return false;
        }
        return ::chdir(dir.c_str()) == 0;
    }

#endif

} // namespace

    bool setWorkingDirectoryFromExecutable(
        std::span<const std::string_view> sentinels) noexcept {
        if (sentinels.empty()) {
            return false;
        }

    std::size_t maxSentinelLen = 0;
    for (std::string_view s : sentinels) {
        assert(!s.empty());
        assert(s.front() != '/');
        assert(s.front() != '\\');
        maxSentinelLen = std::max(maxSentinelLen, s.size());
    }

    assert(kJoinSlack >= (maxSentinelLen + 1));

#ifdef _WIN32
        std::wstring path = getExecutablePathW();
        if (path.empty()) {
            return false;
        }

        stripToParentDir(path);
        if (path.empty()) {
            return false;
        }

        [[maybe_unused]] const std::size_t probeCap = path.capacity();
        assert(probeCap >= path.size() + maxSentinelLen + 1);

        while (true) {
            assert(path.capacity() == probeCap);
            assert(probeCap >= path.size() + maxSentinelLen + 1);

            bool allFound = true;
            for (std::string_view s : sentinels) {
                if (!existsFileW(path, s)) {
                    allFound = false;
                    break;
                }
                assert(path.capacity() == probeCap);
            }

            if (allFound) {
                return setCwdW(path);
            }

            if (!popParent(path)) {
                return false;
            }
        }
#else
        std::string path = getExecutablePathNarrow();
        if (path.empty()) {
            return false;
        }

        stripToParentDir(path);
        if (path.empty()) {
            return false;
        }

        [[maybe_unused]] const std::size_t probeCap = path.capacity();
        assert(probeCap >= path.size() + maxSentinelLen + 1);

        while (true) {
            assert(path.capacity() == probeCap);
            assert(probeCap >= path.size() + maxSentinelLen + 1);

            bool allFound = true;
            for (std::string_view s : sentinels) {
                if (!existsFileNarrow(path, s)) {
                    allFound = false;
                    break;
                }
                assert(path.capacity() == probeCap);
            }

            if (allFound) {
                return setCwdNarrow(path);
            }

            if (!popParent(path)) {
                return false;
            }
        }
#endif
    }

} // namespace core::runtime::entry