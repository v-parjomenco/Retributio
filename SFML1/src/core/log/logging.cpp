#include "pch.h"

#include "core/log/logging.h"
#include "core/log/log_defaults.h"
#include "core/log/log_categories.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <vector>
#include <utility>

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
        static std::once_flag once;
        std::call_once(once, []() {
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
        });
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
            attributes =
                (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY) |
                (BACKGROUND_RED | BACKGROUND_INTENSITY);
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

    {
        std::error_code ec;
        fs::create_directories(fs::path(cfg.logDirectory), ec);
        if (ec) {
            // Если не удалось создать каталог — пишем только в консоль.
            state.config.fileEnabled = false;
            return;
        }
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
        if (cfg.maxFiles == 0) {
            return;
        }
        struct LogFileInfo {
            fs::path path;
            fs::file_time_type time;
        };

        std::vector<LogFileInfo> files;
        std::error_code itEc;
        for (const auto& entry : fs::directory_iterator(logDir, itEc)) {
            std::error_code stEc;
            if (!entry.is_regular_file(stEc) || stEc) {
                continue;
            }
            if (entry.path().extension() != ".log") {
                continue;
            }

            std::error_code tEc;
            const auto t = entry.last_write_time(tEc);
            fs::file_time_type ts = tEc ? fs::file_time_type::min() : t;
            if (tEc && entry.path() == logPath) {
                ts = fs::file_time_type::max();
            }
            files.push_back(LogFileInfo{
                entry.path(),
                ts
            });
        }

        // Если итерация по директории не удалась — просто пропускаем ротацию (логгер работает дальше).
        if (!itEc) {
            std::sort(files.begin(), files.end(),
                      [](const LogFileInfo& a, const LogFileInfo& b) {
                          return a.time > b.time;
                      });

            if (files.size() > cfg.maxFiles) {
                for (std::size_t i = cfg.maxFiles; i < files.size(); ++i) {
                    std::error_code rmEc;
                    fs::remove(files[i].path, rmEc);
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

        // Fast-path gate: после успешного применения конфига макросы могут отсекать
        // форматирование без mutex.
        g_minLevelAtomic.store(state.config.minLevel, std::memory_order_release);
        g_fastGateReady.store(true, std::memory_order_release);

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

        // Fast-path gate update.
        g_minLevelAtomic.store(level, std::memory_order_release);
        g_fastGateReady.store(true, std::memory_order_release);
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

        // Возвращаем "консервативный режим": до следующей инициализации не делаем early-reject
        // по atomic.
        g_fastGateReady.store(false, std::memory_order_release);
    }

    void log(Level level,
             std::string_view category,
             std::string_view message,
             const char* file,
             int line) {        

        // ----------------------------------------------------------------------------------------
        // Fast-path reject БЕЗ mutex:
        // если логгер уже инициализирован (gateReady=true), и уровень ниже порога —
        // то нет смысла брать lock и заниматься форматированием строки лога.
        //
        // Важно: мы всё равно повторно проверим state.config.minLevel под mutex ниже,
        // чтобы:
        //  - корректно обработать гонку с setMinLevel();
        //  - корректно работать в "консервативном режиме" (gateReady=false).
        // ----------------------------------------------------------------------------------------
        if (g_fastGateReady.load(std::memory_order_acquire)) {
            const Level minLevel = g_minLevelAtomic.load(std::memory_order_relaxed);
            if (level < minLevel) {
                return;
            }
        }

        auto& state = getState();
        std::lock_guard<std::mutex> lock(state.mutex);

        ensureInitialized(state);

        // Точная проверка под mutex: гарантирует, что сообщения не "пробьются" при гонке
        // с setMinLevel().
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

    [[noreturn]] void panic(std::string_view category,
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
            g_fastGateReady.store(false, std::memory_order_release);
        }

        // Собираем текст для пользователя (на русском).
        const std::string userMessage = std::format(
            "Категория: {}\n\n{}\n\nФайл: {}\nСтрока: {}",
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

        std::exit(EXIT_FAILURE);
    }

} // namespace core::log