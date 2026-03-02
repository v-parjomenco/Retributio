#include "pch.h"

#include "core/runtime/entry/single_instance_guard.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

#ifdef _WIN32
    // <windows.h> включён через single_instance_guard.h (который включает его явно).
    // Все Win32 API (CreateMutexW, CloseHandle и т.д.) доступны транзитивно.
#else
    #include <cerrno>
    #include <fcntl.h>
    #include <sys/file.h>
    #include <unistd.h>
#endif

namespace core::runtime::entry {

namespace {

#ifdef _WIN32

    // Имя мьютекса в Global\ namespace обеспечивает кроссcессионную защиту.
    // Не выносим в inline constexpr в хедер: это деталь реализации, не публичный контракт.
    constexpr wchar_t kMutexName[] = L"Global\\SkyGuard_Game_Mutex";

#else

    // Имя lock-файла в системной temp-директории. Путь вычисляется в конструкторе.
    constexpr char kLockFileName[] = "skyguard.lock";

    constexpr std::size_t kLockPathHardCap = 4096;

    const char* pickTempDir() noexcept {
        // std::filesystem::temp_directory_path может аллоцировать и бросать.
        // Для entry-слоя это недопустимо: строим путь без аллокаций.
        const char* dir = std::getenv("TMPDIR");
        if (dir != nullptr && dir[0] != '\0') {
            return dir;
        }
        dir = std::getenv("TMP");
        if (dir != nullptr && dir[0] != '\0') {
            return dir;
        }
        dir = std::getenv("TEMP");
        if (dir != nullptr && dir[0] != '\0') {
            return dir;
        }
        return "/tmp";
    }

    bool buildLockPath(char* out, std::size_t outSize) noexcept {
        if (out == nullptr || outSize == 0u) {
            return false;
        }

        const char* const dir = pickTempDir();
        const std::size_t dirLen = std::strlen(dir);
        if (dirLen == 0u) {
            return false;
        }

        const bool hasSlash = (dir[dirLen - 1] == '/');
        const int written = hasSlash
            ? std::snprintf(out, outSize, "%s%s", dir, kLockFileName)
            : std::snprintf(out, outSize, "%s/%s", dir, kLockFileName);

        if (written <= 0) {
            return false;
        }
        return static_cast<std::size_t>(written) < outSize;
    }

#endif

} // namespace

    SingleInstanceGuard::SingleInstanceGuard() noexcept {
#ifdef _WIN32
        mutex_ = CreateMutexW(nullptr, TRUE, kMutexName);

        if (mutex_ == nullptr) {
            // CreateMutexW вернул nullptr — системная ошибка (например, ERROR_ACCESS_DENIED).
            // Политика: fail-fast без Local\ fallback (маскирует двойной запуск в RDS).
            status_ = SingleInstanceStatus::OsError;
            return;
        }

        // ВАЖНО: GetLastError() вызывается сразу после CreateMutexW — до любых других
        // Win32-вызовов, которые могут перезаписать код ошибки потока.
        const DWORD err = GetLastError();

        if (err == ERROR_ALREADY_EXISTS) {
            // Мьютекс уже существует — другой экземпляр держит его.
            // Освобождаем handle: он нам не нужен, мьютекс закрывает ОС.
            CloseHandle(std::exchange(mutex_, nullptr));
            status_ = SingleInstanceStatus::AlreadyRunning;
        } else {
            // err == 0 или иной успешный код: мы создали мьютекс и являемся владельцем.
            status_ = SingleInstanceStatus::Acquired;
        }
#else
        char lockPath[kLockPathHardCap]{};
        if (!buildLockPath(lockPath, sizeof(lockPath))) {
            status_ = SingleInstanceStatus::OsError;
            return;
        }

        fd_ = ::open(lockPath, O_CREAT | O_RDONLY, 0644);
        if (fd_ < 0) {
            // Не удалось открыть/создать lock-файл.
            status_ = SingleInstanceStatus::OsError;
            return;
        }

        if (::flock(fd_, LOCK_EX | LOCK_NB) == 0) {
            // Эксклюзивный lock получен — мы единственный экземпляр.
            status_ = SingleInstanceStatus::Acquired;
        } else {
            // errno читается немедленно: любой вызов между flock и errno
            // перезапишет код ошибки потока.
            const int flockErr = errno;
            // Закрываем fd_: lock не получен, он нам не нужен.
            ::close(std::exchange(fd_, -1));
            // EWOULDBLOCK (= EAGAIN на части платформ): lock держит другой процесс.
            // Всё остальное: деградация среды (ENOLCK, EIO, EROFS и т.д.) — OsError.
            status_ = (flockErr == EWOULDBLOCK || flockErr == EAGAIN)
                ? SingleInstanceStatus::AlreadyRunning
                : SingleInstanceStatus::OsError;
        }
#endif
    }

    SingleInstanceGuard::~SingleInstanceGuard() noexcept {
        release();
    }

    SingleInstanceGuard::SingleInstanceGuard(SingleInstanceGuard&& other) noexcept
        : status_(std::exchange(other.status_, SingleInstanceStatus::Inactive))
#ifdef _WIN32
        , mutex_(std::exchange(other.mutex_, nullptr))
#else
        , fd_(std::exchange(other.fd_, -1))
#endif
    {}

    SingleInstanceGuard& SingleInstanceGuard::operator=(SingleInstanceGuard&& other) noexcept {
        if (this != &other) {
            // Освобождаем текущий ресурс перед захватом нового.
            release();
            status_ = std::exchange(other.status_, SingleInstanceStatus::Inactive);
#ifdef _WIN32
            mutex_ = std::exchange(other.mutex_, nullptr);
#else
            fd_ = std::exchange(other.fd_, -1);
#endif
        }
        return *this;
    }

    SingleInstanceStatus SingleInstanceGuard::status() const noexcept {
        return status_;
    }

    void SingleInstanceGuard::release() noexcept {
#ifdef _WIN32
        if (mutex_ != nullptr) {
            CloseHandle(std::exchange(mutex_, nullptr));
        }
#else
        if (fd_ >= 0) {
            // Снимаем flock перед закрытием для явной семантики освобождения.
            // ОС снимает lock автоматически при close(), но явный LOCK_UN — лучше.
            ::flock(fd_, LOCK_UN);
            ::close(fd_);
            fd_ = -1;
            // Lock-файл в temp-папке намеренно не удаляем:
            //  - блокировка управляется через flock по inode, не по имени файла;
            //  - unlink создаёт гонку: новый процесс может открыть файл по имени,
            //    а удалённый inode всё ещё держится старым fd другого процесса.
        }
#endif
        status_ = SingleInstanceStatus::Inactive;
    }

} // namespace core::runtime::entry