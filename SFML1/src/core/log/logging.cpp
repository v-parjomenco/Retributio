#include "pch.h"

#include "core/log/logging.h"
#include "core/log/log_defaults.h"
#include "core/log/log_categories.h"

#include <chrono>
#include <ctime>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

#ifdef _WIN32
    #include <windows.h>
#endif

namespace core::log {

namespace {

    // --------------------------------------------------------------------------------------------
    // Внутреннее состояние логгера
    // --------------------------------------------------------------------------------------------

    struct LoggerState {
        std::mutex mutex;
        bool   initialized = false;
        Config config{};
        std::ofstream file;

        LoggerState() = default;
        LoggerState(const LoggerState&)            = delete;
        LoggerState& operator=(const LoggerState&) = delete;
        LoggerState(LoggerState&&)                 = delete;
        LoggerState& operator=(LoggerState&&)      = delete;
    };

    LoggerState& getState() {
        static LoggerState state;
        return state;
    }

    // --------------------------------------------------------------------------------------------
    // Вспомогательные функции
    // --------------------------------------------------------------------------------------------

    std::string makeTimestamp() {
        using namespace std::chrono;

        const auto now       = system_clock::now();
        const auto timeT     = system_clock::to_time_t(now);

    std::tm localTime{};
    #ifdef _WIN32
        localtime_s(&localTime, &timeT);
    #elif defined(__unix__) || defined(__APPLE__)
        localtime_r(&timeT, &localTime);
    #else
        localTime = *std::localtime(&timeT);
    #endif

        std::ostringstream oss;
        oss << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");

        const auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
        oss << '.' << std::setfill('0') << std::setw(3) << ms.count();

        return oss.str();
    }

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
            0
        );

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
            required
        );

        if (converted <= 0) {
            return L"<UTF-8 to UTF-16 conversion failed>";
        }

        return result;
    }

    void configureConsoleUtf8Once() {
        static bool configured = false;
        if (configured) {
            return;
        }

        configured = true;

        // Включаем UTF-8 и поддержку ANSI-цветов, если возможно.
        SetConsoleOutputCP(CP_UTF8);

        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD mode = 0;
            if (GetConsoleMode(hOut, &mode)) {
                mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(hOut, mode);
            }
        }
    }

    #endif // _WIN32

    void printToConsole(Level level, const std::string& message) {
    #ifdef _WIN32
        configureConsoleUtf8Once();

        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut == INVALID_HANDLE_VALUE) {
            // Фоллбэк — обычный stdout без цветов.
            std::cout << message << '\n' << std::flush; 
            return;
        }

        WORD attributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;

        switch (level) {
        case Level::Trace:
            attributes = FOREGROUND_INTENSITY;
            break;
        case Level::Debug:
            attributes = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
            break;
        case Level::Info:
            attributes = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            break;
        case Level::Warning:
            attributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            break;
        case Level::Error:
            attributes = FOREGROUND_RED | FOREGROUND_INTENSITY;
            break;
        case Level::Critical:
            attributes = FOREGROUND_RED | FOREGROUND_INTENSITY | BACKGROUND_INTENSITY;
            break;
        }

        CONSOLE_SCREEN_BUFFER_INFO oldInfo{};
        GetConsoleScreenBufferInfo(hOut, &oldInfo);

        SetConsoleTextAttribute(hOut, attributes);
        std::cout << message << '\n' << std::flush;
        SetConsoleTextAttribute(hOut, oldInfo.wAttributes);

        // Дублируем в OutputDebugString, чтобы было видно в Visual Studio.
        const std::wstring wide = utf8ToWide(message + "\n");
        OutputDebugStringW(wide.c_str());
    #else
        // ANSI-цвета для Unix-подобных систем.
        const char* color = "\033[0m";

        switch (level) {
        case Level::Trace:    color = "\033[90m";   break; // серый
        case Level::Debug:    color = "\033[36m";   break; // циан
        case Level::Info:     color = "\033[32m";   break; // зелёный
        case Level::Warning:  color = "\033[33m";   break; // жёлтый
        case Level::Error:    color = "\033[31m";   break; // красный
        case Level::Critical: color = "\033[1;41m"; break; // ярко-красный фон
        }

        std::cout << color << message << "\033[0m\n" << std::flush;
    #endif
    }

    void openLogFileLocked(LoggerState& state, const Config& cfg) {
        namespace fs = std::filesystem;

        if (!cfg.fileEnabled) {
            return;
        }

        try {
            fs::create_directories(fs::path(cfg.logDirectory));
        } catch (...) {
            // Если не удалось создать каталог — пишем только в консоль.
            state.config.fileEnabled = false;
            return;
        }

        const auto now   = std::chrono::system_clock::now();
        const auto timeT = std::chrono::system_clock::to_time_t(now);

    std::tm tm{};
    #ifdef _WIN32
        localtime_s(&tm, &timeT);
    #elif defined(__unix__) || defined(__APPLE__)
        localtime_r(&timeT, &tm);
    #else
        tm = *std::localtime(&timeT);
    #endif

        std::ostringstream name;
        name << "engine_"
             << std::put_time(&tm, "%Y%m%d_%H%M%S")
             << ".log";

        const fs::path logDir  = fs::path(cfg.logDirectory);
        const fs::path logPath = logDir / name.str();

        state.file.open(logPath, std::ios::out | std::ios::app);
        if (!state.file.is_open()) {
            state.config.fileEnabled = false;
            return;
        }

        // Небольшой заголовок лога.
        state.file << "========================================\n";
        state.file << "  Engine Log\n";
        state.file << "  Started: " << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << '\n';
        state.file << "========================================\n\n";
        state.file.flush();

        // Ротация: удаляем самые старые файлы, если их больше maxFiles.
        if (cfg.maxFiles > 0) {
            std::vector<fs::directory_entry> entries;
            for (const auto& entry : fs::directory_iterator(logDir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".log") {
                    entries.push_back(entry);
                }
            }

            std::sort(entries.begin(), entries.end(),
                      [](const fs::directory_entry& a, const fs::directory_entry& b) {
                          return fs::last_write_time(a) > fs::last_write_time(b);
                      });

            if (entries.size() > cfg.maxFiles) {
                for (std::size_t i = cfg.maxFiles; i < entries.size(); ++i) {
                    std::error_code ec;
                    fs::remove(entries[i].path(), ec);
                }
            }
        }
    }

    void initLocked(LoggerState& state, const Config& cfg) {
        // Оптимизация: если конфиг не изменился, не переоткрываем файл
        if (state.initialized && state.config == cfg) {
            return;
        }

        // Закрываем старый файл, если был открыт
        if (state.file.is_open()) {
            state.file.flush();
            state.file.close();
        }

        state.config = cfg;
        openLogFileLocked(state, cfg);

    #ifdef _WIN32
        if (state.config.consoleEnabled) {
            configureConsoleUtf8Once();
        }
    #endif

        state.initialized = true;
    }

    void ensureInitialized(LoggerState& state) {
        // ВАЖНО:
        // Предполагается, что вызывающий код уже держит state.mutex.
        // Сейчас ensureInitialized вызывается только из log()/setMinLevel под lock_guard.
        if (state.initialized) {
            return;
        }

        const Config cfg = defaults::makeDefaultConfig();
        initLocked(state, cfg);
    }

    } // namespace

    // --------------------------------------------------------------------------------------------
    // Публичный API
    // --------------------------------------------------------------------------------------------

    void init(const Config& config) {
        auto& state = getState();
        std::lock_guard<std::mutex> lock(state.mutex);
        initLocked(state, config);
    }

    void setMinLevel(Level level) {
        auto& state = getState();
        std::lock_guard<std::mutex> lock(state.mutex);

        ensureInitialized(state);
        state.config.minLevel = level;
    }

    void shutdown() {
        auto& state = getState();
        std::lock_guard<std::mutex> lock(state.mutex);

        if (!state.initialized) {
            return;
        }

        if (state.file.is_open()) {
            state.file.flush();
            state.file.close();
        }

        state.initialized = false;
    }

    void log(Level level,
             std::string_view category,
             std::string_view message,
             const char* file,
             int line) {
        auto& state = getState();
        std::lock_guard<std::mutex> lock(state.mutex);

        ensureInitialized(state);

        if (level < state.config.minLevel) {
            return;
        }

        const std::string      timestamp = makeTimestamp();
        const std::string_view levelName = toString(level);

        std::string lineText;
        try {
            lineText = std::format(
                "[{0}] [{1}] [{2}] {3} ({4}:{5})",
                timestamp,
                levelName,
                category,
                message,
                file ? file : "",
                line
            );
        } catch (const std::format_error& e) {
            // Фоллбэк, если форматирование сломалось.
            lineText = "[LOG FORMAT ERROR] ";
            lineText += e.what();
            lineText += " | original: ";
            lineText.append(message.begin(), message.end());
        }

        if (state.config.consoleEnabled) {
            printToConsole(level, lineText);
        }

        if (state.config.fileEnabled && state.file.is_open()) {
            state.file << lineText << '\n';

            if (state.config.flushOnError && level >= Level::Error) {
                state.file.flush();
            }
        }
    }

    void panic(std::string_view category,
               std::string_view message,
               const char* file,
               int line) {
        // Пишем в лог как CRITICAL.
        log(Level::Critical, category, message, file, line);

        // Сбрасываем и закрываем файл.
        auto& state = getState();
        {
            std::lock_guard<std::mutex> lock(state.mutex);
            if (state.file.is_open()) {
                state.file.flush();
                state.file.close();
            }
            state.initialized = false;
        }

        // Собираем текст для пользователя (на русском).
        const std::string userMessage = std::format(
            "Критическая ошибка.\n\nКатегория: {}\n\n{}\n\nФайл: {}\nСтрока: {}",
            category,
            message,
            file ? file : "",
            line
        );

    #ifdef _WIN32
        const std::wstring wide = utf8ToWide(userMessage);
        MessageBoxW(
            nullptr,
            wide.c_str(),
            L"Критическая ошибка",
            MB_OK | MB_ICONERROR | MB_SETFOREGROUND | MB_TOPMOST
        );
    #else
        std::cerr << "\n========================================\n";
        std::cerr << "КРИТИЧЕСКАЯ ОШИБКА\n\n";
        std::cerr << userMessage << '\n';
        std::cerr << "========================================\n\n";
    #endif

        // Аккуратный выход: деструкторы статиков/atexit и т.п.
        std::exit(EXIT_FAILURE);
    }

} // namespace core::log