// ================================================================================================
// File: main_skyguard.cpp
// Purpose: Entry point for the SkyGuard game (single-game build for now).
// Notes:
//  - Sets up working directory, logging, crash safety net and single-instance guard.
//  - Runs the SkyGuard::Game main loop with robust error reporting.
// ================================================================================================

#include "pch.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <new>
#include <string>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <fcntl.h>      // open
    #include <sys/file.h>   // flock
    #include <unistd.h>     // close
#endif

#include "core/debug/debug_config.h"
#include "core/log/log_defaults.h"
#include "core/log/log_macros.h"
#include "core/log/logging.h"
#include "game/skyguard/game.h"

namespace {

    // --------------------------------------------------------------------------------------------
    // Build info (версия, дата сборки, git-коммит)
    // --------------------------------------------------------------------------------------------

    #ifndef GIT_COMMIT_HASH
        #define GIT_COMMIT_HASH "<unknown>"
    #endif

    struct BuildInfo {
        const char* version   = "0.1.0-dev";
        const char* buildDate = __DATE__ " " __TIME__;
    #ifdef NDEBUG
        const char* buildType = "Release";
    #else
        const char* buildType = "Debug";
    #endif
        const char* gitCommit = GIT_COMMIT_HASH;
    };

    void logBuildInfo()
    {
        const BuildInfo info{};
        LOG_INFO(core::log::cat::Engine,
                 "Build info: version={}, type={}, built at {}, commit={}",
                 info.version,
                 info.buildType,
                 info.buildDate,
                 info.gitCommit);
    }

    // --------------------------------------------------------------------------------------------
    // ScopedTimer — измерение времени крупных операций (инициализация, сессия игры)
    // --------------------------------------------------------------------------------------------

    class ScopedTimer {
    public:
        explicit ScopedTimer(std::string name) noexcept
            : name_(std::move(name))
            , start_(std::chrono::steady_clock::now())
        {}

        ~ScopedTimer()
        {
            using namespace std::chrono;
            const auto end     = steady_clock::now();
            const auto elapsed = duration_cast<milliseconds>(end - start_).count();

            LOG_DEBUG(core::log::cat::Performance,
                      "[TIMER] {} took {} ms",
                      name_,
                      elapsed);
        }

    private:
        std::string                            name_;
        std::chrono::steady_clock::time_point  start_;
    };

    // --------------------------------------------------------------------------------------------
    // SingleInstanceGuard — защита от двойного запуска (Windows + Unix-подобные системы)
    // --------------------------------------------------------------------------------------------

    class SingleInstanceGuard {
    public:
        SingleInstanceGuard()
        {
    #ifdef _WIN32
            mutex_ = CreateMutexW(nullptr, TRUE, L"Global\\SkyGuard_Game_Mutex");
            if (mutex_ == nullptr) {
                // На этом этапе core::log уже инициализирован (см. порядок в main),
                // поэтому LOG_WARN безопасен: LOG_WARN в таком случае просто не блокирует запуск.
                LOG_WARN(core::log::cat::Engine,
                         "Не удалось создать именованный мьютекс для единственного экземпляра "
                         "(GetLastError() = {}). Продолжение без защиты.",
                         static_cast<unsigned long>(GetLastError()));
                acquired_ = true;
                return;
            }

            const DWORD err = GetLastError();
            if (err == ERROR_ALREADY_EXISTS) {
                // Другой экземпляр уже держит мьютекс — аккуратно выходим.
                acquired_ = false;

                MessageBoxW(
                    nullptr,
                    L"Игра SkyGuard уже запущена.",
                    L"SkyGuard",
                    MB_OK | MB_ICONWARNING | MB_SETFOREGROUND | MB_TOPMOST
                );
            } else {
                acquired_ = true;
            }
    #else
            try {
                const auto lockPath =
                    std::filesystem::temp_directory_path() / "skyguard.lock";
                lockPath_ = lockPath.string();

                fd_ = ::open(lockPath_.c_str(), O_CREAT | O_RDWR, 0644);
                if (fd_ >= 0) {
                    if (::flock(fd_, LOCK_EX | LOCK_NB) == 0) {
                        // Эксклюзивный lock получен — мы единственный экземпляр.
                        acquired_ = true;
                    } else {
                        // Lock взять не удалось — кто-то уже держит файл.
                        acquired_ = false;
                        std::cerr << "SkyGuard is already running.\n";
                        ::close(fd_);
                        fd_ = -1;
                    }
                } else {
                    // Не удалось открыть lock-файл — считаем, что не блокируем запуск.
                    acquired_ = true;
                }
            } catch (...) {
                // Ошибки файловой системы не должны полностью ломать запуск игры.
                acquired_ = true;
            }
    #endif
        }

        ~SingleInstanceGuard()
        {
    #ifdef _WIN32
            if (mutex_ != nullptr) {
                // Для паттерна "single instance" достаточно закрыть handle.
                // ОС сама освободит объект и снимет владение при закрытии последнего handle.
                CloseHandle(mutex_);
                mutex_ = nullptr;
            }
    #else
            if (fd_ >= 0) {
                ::flock(fd_, LOCK_UN);
                ::close(fd_);
                fd_ = -1;

                // lock-файл в temp-папке намеренно оставляем:
                //  - блокировка управляется через flock по inode;
                //  - наличие/отсутствие файла не критично для корректности;
                //  - так мы избегаем потенциальных гонок с unlink().
            }
    #endif
        }

        [[nodiscard]] bool acquired() const noexcept { return acquired_; }

    private:
        bool acquired_ = false;

    #ifdef _WIN32
        HANDLE mutex_ = nullptr;
    #else
        int         fd_       = -1;
        std::string lockPath_;
    #endif
    };

    // --------------------------------------------------------------------------------------------
    // Фиксация рабочего каталога: ищем папку, где лежит assets/, относительно exe
    // --------------------------------------------------------------------------------------------

    void fixWorkingDirectory(int argc, char* argv[])
    {
        namespace fs = std::filesystem;

        fs::path exePath;

        try {
            if (argc > 0 && argv != nullptr && argv[0] != nullptr) {
                exePath = fs::path(argv[0]);

                if (!exePath.is_absolute()) {
                    exePath = fs::absolute(exePath);
                }
            }
        } catch (...) {
            // В крайнем случае просто остаёмся в текущем каталоге.
            return;
        }

        if (exePath.empty()) {
            return;
        }

        fs::path probe = exePath.parent_path();
        fs::path root  = probe.root_path();

        std::error_code ec;

        while (!probe.empty()) {
            const fs::path assetsDir = probe / "assets";
            if (fs::exists(assetsDir, ec) && fs::is_directory(assetsDir, ec)) {
                fs::current_path(probe, ec);
                return;
            }

            if (probe == root) {
                break;
            }

            probe = probe.parent_path();
        }
    }

    // --------------------------------------------------------------------------------------------
    // Инициализация логгера и terminate-handler
    // --------------------------------------------------------------------------------------------

    void initializeLogging()
    {
        core::log::Config cfg = core::log::defaults::makeDefaultConfig();
        core::log::init(cfg);

        LOG_INFO(core::log::cat::Engine,
                 "Logging initialized (minLevel={}, console={}, file={}, dir='{}', maxFiles={})",
                 static_cast<int>(cfg.minLevel),
                 cfg.consoleEnabled,
                 cfg.fileEnabled,
                 cfg.logDirectory,
                 cfg.maxFiles);
    }

    void installTerminateHandler()
    {
        using core::log::cat::Engine;

        std::set_terminate([] {
            // Защита от рекурсивных вызовов std::terminate 
            // (например, если внутри LOG_PANIC что-то упадёт).
            static std::atomic<bool> terminating{false};

            if (terminating.exchange(true)) {
                // Уже в процессе завершения — жёстко выходим, чтобы не уйти в рекурсию.
                std::abort();
            }

            try {
                if (auto ex = std::current_exception()) {
                    try {
                        std::rethrow_exception(ex);
                    } catch (const std::exception& e) {
                        LOG_PANIC(Engine,
                                  "Unhandled exception (std::terminate): {}",
                                  e.what());
                    } catch (...) {
                        LOG_PANIC(Engine,
                                  "Unhandled non-std exception (std::terminate)");
                    }
                } else {
                    LOG_PANIC(Engine,
                              "std::terminate called without active exception");
                }
            } catch (...) {
                // Если что-то пошло совсем вразнос, делаем жёсткий abort.
                std::abort();
            }
        });
    }

    // --------------------------------------------------------------------------------------------
    // Локальный UX-слой для сообщений пользователю и паузы при выходе
    // --------------------------------------------------------------------------------------------

    /// UTF-8 → UTF-16 конвертер и message box только для main (Windows)
#ifdef _WIN32
    std::wstring utf8ToWide(const std::string& str)
    {
        if (str.empty()) {
            return L"";
        }

        const int required =
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.c_str(), -1, nullptr, 0);
        if (required <= 0) {
            return L"[UTF-8 conversion error]";
        }

        std::wstring result(static_cast<std::wstring::size_type>(required), L'\0');
        const int converted =
            MultiByteToWideChar(CP_UTF8,
                                MB_ERR_INVALID_CHARS,
                                str.c_str(),
                                -1,
                                result.data(),
                                required);
        if (converted <= 0) {
            return L"[UTF-8 conversion error]";
        }

        if (!result.empty() && result.back() == L'\0') {
            result.pop_back();
        }

        return result;
    }

    void showNativeMessageBox(const std::string& text,
                              const std::string& title,
                              UINT type)
    {
        MessageBoxW(nullptr,
                    utf8ToWide(text).c_str(),
                    utf8ToWide(title).c_str(),
                    type | MB_SETFOREGROUND | MB_TOPMOST);
    }
#endif

    /// Пользовательский диалог об ошибке (верхний уровень)
    void showUserErrorDialog(const std::string& text)
    {
#ifdef _WIN32
        showNativeMessageBox(text, "Ошибка", MB_OK | MB_ICONERROR);
#else
        std::cerr << "[Ошибка] " << text << std::endl;
#endif
    }

    /// Пауза при выходе (зависит от core::config::DEBUG_HOLD_ON_EXIT)
    void holdOnExit()
    {
        if constexpr (::core::config::DEBUG_HOLD_ON_EXIT) {
#ifdef _WIN32
            showNativeMessageBox("Нажмите OK, чтобы выйти.", "Завершение", MB_OK);
#else
            std::cout << "\nНажмите Enter, чтобы выйти..." << std::endl;
            std::cin.get();
#endif
        }
    }

} // namespace

// ================================================================================================
// main
// ================================================================================================

int main(int argc, char* argv[])
{
    // 1. Ставим terminate-handler как можно раньше, чтобы поймать любые ранние аварии.
    installTerminateHandler();

    // 2. Фиксируем рабочий каталог относительно exe, чтобы assets/ находились корректно.
    fixWorkingDirectory(argc, argv);

    // 3. Инициализируем логгер и сразу логируем build info + рабочий каталог.
    initializeLogging();
    logBuildInfo();

    try {
        LOG_INFO(core::log::cat::Engine,
                 "Working directory: {}",
                 std::filesystem::current_path().string());
    } catch (...) {
        // Если вдруг std::filesystem бросит — не даём этому убить игру.
    }

    // 4. Single-instance guard — защищаемся от двойного запуска.
    SingleInstanceGuard instanceGuard;
    if (!instanceGuard.acquired()) {
        // Тут уже могли показать MessageBox (Windows) или stderr (Linux).
        LOG_WARN(core::log::cat::Engine,
                 "Другой экземпляр приложения уже запущен. Выход.");
        try {
            core::log::shutdown();
        } catch (...) {
            // Игнорируем ошибки shutdown.
        }
        holdOnExit();              /// Локальная пауза при выходе
        return EXIT_SUCCESS;
    }

    // 5. Запускаем игру под safety-net из try/catch.
    try {
        LOG_INFO(core::log::cat::Engine, "Launching SkyGuard...");

        ScopedTimer timer("Game session");

        game::skyguard::Game game;
        game.run();

        LOG_INFO(core::log::cat::Engine, "SkyGuard game finished cleanly.");
    } catch (const std::exception& e) {
        LOG_ERROR(core::log::cat::Engine,
                  "Необработанное исключение в main(): {}",
                  e.what());

        /// Диалог + пауза только на верхнем уровне
        showUserErrorDialog(
            "Произошла непредвиденная ошибка.\n"
            "Подробности см. в лог-файле в папке 'logs'.");
        try {
            core::log::shutdown();
        } catch (...) {
            // Ничего уже не поделаешь.
        }
        holdOnExit();
        return EXIT_FAILURE;
    } catch (...) {
        LOG_ERROR(core::log::cat::Engine,
                  "Необработанное нестандартное исключение в main().");

        showUserErrorDialog(
            "Произошла неизвестная ошибка.\n"
            "Подробности см. в лог-файле в папке 'logs'.");
        try {
            core::log::shutdown();
        } catch (...) {
            // Игнорируем.
        }
        holdOnExit();
        return EXIT_FAILURE;
    }

    // 6. Аккуратный shutdown логгера при нормальном завершении.
    try {
        core::log::shutdown();
    } catch (...) {
        // Ошибки при завершении логгера нам уже не критичны.
    }

    holdOnExit();  /// тот же UX при нормальном выходе
    return EXIT_SUCCESS;
}