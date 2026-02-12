#include "pch.h"

#include "core/log/logging.h"
#include "core/log/log_defaults.h"
#include "core/log/log_categories.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <mutex>
#include <vector>

#ifdef _WIN32
    #include <windows.h>
#endif

namespace core::log {

namespace {

    // --------------------------------------------------------------------------------------------
    // Внутреннее состояние логгера
    // --------------------------------------------------------------------------------------------

    struct LoggerState {
        std::mutex    mutex;
        bool          initialized = false;
        Config        config{};
        std::ofstream file;
        std::string   scratch;            ///< Переиспользуемый буфер строки; reserve один раз, clear на каждую запись.

        LoggerState() { scratch.reserve(1024); }

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
    // Timestamp — стековый буфер, без iostream, без аллокаций.
    // Формат: "2025-06-15 14:30:45.123" (23 символа).
    // --------------------------------------------------------------------------------------------

    std::string_view writeTimestamp(char* buf, std::size_t bufSize) noexcept {
        using namespace std::chrono;

        const auto now   = system_clock::now();
        const auto timeT = system_clock::to_time_t(now);

        std::tm localTime{};
    #ifdef _WIN32
        localtime_s(&localTime, &timeT);
    #elif defined(__unix__) || defined(__APPLE__)
        localtime_r(&timeT, &localTime);
    #else
        localTime = *std::localtime(&timeT);
    #endif

        // "2025-06-15 14:30:45" = 19 символов
        const std::size_t written = std::strftime(buf, bufSize, "%Y-%m-%d %H:%M:%S", &localTime);

        // strftime возвращает 0 при ошибке — содержимое буфера indeterminate (стандарт C).
        // В этом случае не дописываем миллисекунды и возвращаем пустой view.
        if (written == 0) {
            return {};
        }

        // Дописываем ".XXX" миллисекунды.
        const auto ms      = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
        const int  msCount = static_cast<int>(ms.count());

        if (written + 4 < bufSize) {
            buf[written]     = '.';
            buf[written + 1] = static_cast<char>('0' + (msCount / 100));
            buf[written + 2] = static_cast<char>('0' + (msCount / 10) % 10);
            buf[written + 3] = static_cast<char>('0' + (msCount % 10));
            return {buf, written + 4};
        }

        return {buf, written};
    }

    // --------------------------------------------------------------------------------------------
    // Хелперы конверта — собирают префикс/суффикс строки лога в scratch-буфер.
    // Без std::format на конверте — ручной append, нулевой overhead парсинга.
    // --------------------------------------------------------------------------------------------

    /// Очищает scratch и пишет: "[timestamp] [LEVEL] [category] "
    void buildEnvelopePrefix(std::string& buf, Level level, std::string_view category) {
        char tsBuf[32];
        const auto ts        = writeTimestamp(tsBuf, sizeof(tsBuf));
        const auto levelName = toString(level);

        buf.clear();
        buf += '[';
        buf.append(ts.data(), ts.size());
        buf += "] [";
        buf.append(levelName.data(), levelName.size());
        buf += "] [";
        buf.append(category.data(), category.size());
        buf += "] ";
    }

    /// Дописывает " (file:line)" — только в Debug-сборках; в Release — no-op.
    void buildEnvelopeSuffix([[maybe_unused]] std::string& buf,
                             [[maybe_unused]] const char* file,
                             [[maybe_unused]] int line) {
    #ifdef _DEBUG
        if (file && file[0] != '\0') {
            buf += " (";
            buf += file;
            buf += ':';

            char lineBuf[16];
            const auto [ptr, ec] = std::to_chars(lineBuf, lineBuf + sizeof(lineBuf), line);
            buf.append(lineBuf, ptr);

            buf += ')';
        }
    #endif
    }

    // --------------------------------------------------------------------------------------------
    // Вывод в консоль с цветами. Политика flush: только на Error+.
    // --------------------------------------------------------------------------------------------

#ifdef _WIN32

    std::wstring utf8ToWide(std::string_view text) {
        if (text.empty()) {
            return {};
        }

        const int required = MultiByteToWideChar(
            CP_UTF8, 0,
            text.data(), static_cast<int>(text.size()),
            nullptr, 0
        );

        if (required <= 0) {
            return L"<UTF-8 to UTF-16 conversion failed>";
        }

        std::wstring result;
        result.resize(static_cast<std::size_t>(required));

        const int converted = MultiByteToWideChar(
            CP_UTF8, 0,
            text.data(), static_cast<int>(text.size()),
            result.data(), required
        );

        if (converted <= 0) {
            return L"<UTF-8 to UTF-16 conversion failed>";
        }

        return result;
    }

    void configureConsoleUtf8Once() {
        static std::once_flag once;
        std::call_once(once, []() {
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

    WORD colorForLevel(Level level) {
        switch (level) {
        case Level::Trace:
            return FOREGROUND_INTENSITY;
        case Level::Debug:
            return FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        case Level::Info:
            return FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        case Level::Warning:
            return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        case Level::Error:
            return FOREGROUND_RED | FOREGROUND_INTENSITY;
        case Level::Critical:
            return (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY)
                 | (BACKGROUND_RED | BACKGROUND_INTENSITY);
        }
        return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    }

    void printToConsole(Level level, std::string_view message) {
        configureConsoleUtf8Once();

        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut == INVALID_HANDLE_VALUE) {
            std::cout << message << '\n';
            return;
        }

        CONSOLE_SCREEN_BUFFER_INFO oldInfo{};
        GetConsoleScreenBufferInfo(hOut, &oldInfo);

        SetConsoleTextAttribute(hOut, colorForLevel(level));
        std::cout << message << '\n';
        SetConsoleTextAttribute(hOut, oldInfo.wAttributes);

        // Дублируем в OutputDebugString, чтобы было видно в Visual Studio.
        const std::wstring wide = utf8ToWide(message);
        OutputDebugStringW(wide.c_str());
        OutputDebugStringW(L"\n");
    }

#else // Unix

    void printToConsole(Level level, std::string_view message) {
        const char* color = "\033[0m";

        switch (level) {
        case Level::Trace:    color = "\033[90m";   break; // серый
        case Level::Debug:    color = "\033[36m";   break; // циан
        case Level::Info:     color = "\033[32m";   break; // зелёный
        case Level::Warning:  color = "\033[33m";   break; // жёлтый
        case Level::Error:    color = "\033[31m";   break; // красный
        case Level::Critical: color = "\033[1;41m"; break; // ярко-красный фон
        }

        std::cout << color << message << "\033[0m\n";
    }

#endif // _WIN32

    // --------------------------------------------------------------------------------------------
    // Emit — записывает готовую строку в консоль + файл. Flush только на Error+.
    // Предусловие: mutex захвачен, scratch содержит готовую строку.
    // --------------------------------------------------------------------------------------------

    void emitLine(LoggerState& state, Level level) {
        const bool shouldFlush = state.config.flushOnError && level >= Level::Error;

        if (state.config.consoleEnabled) {
            printToConsole(level, state.scratch);
            if (shouldFlush) {
                std::cout.flush();
            }
        }

        if (state.config.fileEnabled && state.file.is_open()) {
            state.file << state.scratch << '\n';
            if (shouldFlush) {
                state.file.flush();
            }
        }
    }

    // --------------------------------------------------------------------------------------------
    // Безопасные хелперы strftime (защита от UB при ошибке).
    // --------------------------------------------------------------------------------------------

    /// Безопасная запись fallback-строки в C-буфер с null-терминатором.
    void writeCStrFallback(char* buf,
                           std::size_t bufSize,
                           std::string_view fallback) noexcept {
        if (bufSize == 0) {
            return;
        }
        const std::size_t n = (fallback.size() < (bufSize - 1))
                                  ? fallback.size()
                                  : (bufSize - 1);
        for (std::size_t i = 0; i < n; ++i) {
            buf[i] = fallback[i];
        }
        buf[n] = '\0';
    }

    /// strftime с fallback: если strftime вернул 0 — пишет fallback и возвращает false.
    bool strftimeOrFallback(char* buf,
                            std::size_t bufSize,
                            const char* fmt,
                            const std::tm& tm,
                            std::string_view fallback) noexcept {
        const std::size_t written = std::strftime(buf, bufSize, fmt, &tm);
        if (written == 0) {
            writeCStrFallback(buf, bufSize, fallback);
            return false;
        }
        return true;
    }

    // --------------------------------------------------------------------------------------------
    // Открытие лог-файла и ротация
    // --------------------------------------------------------------------------------------------

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

        // Формируем имя файла. Fallback гарантирует валидный null-terminated C-string.
        char nameBuf[64];
        strftimeOrFallback(nameBuf, sizeof(nameBuf),
                           "engine_%Y%m%d_%H%M%S.log", tm,
                           "engine_unknown.log");

        const fs::path logDir  = fs::path(cfg.logDirectory);
        const fs::path logPath = logDir / nameBuf;

        state.file.open(logPath, std::ios::out | std::ios::app);
        if (!state.file.is_open()) {
            state.config.fileEnabled = false;
            return;
        }

        // Заголовок лог-файла.
        char startBuf[32];
        strftimeOrFallback(startBuf, sizeof(startBuf),
                           "%Y-%m-%d %H:%M:%S", tm,
                           "unknown");

        state.file << "========================================\n"
                   << "  Engine Log\n"
                   << "  Started: " << startBuf << '\n'
                   << "========================================\n\n";
        state.file.flush();

        // Ротация: удаляем самые старые файлы, если их больше maxFiles.
        if (cfg.maxFiles == 0) {
            return;
        }

        struct LogFileInfo {
            fs::path             path;
            fs::file_time_type   time;
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
            files.push_back({entry.path(), ts});
        }

        // Если итерация по директории не удалась — пропускаем ротацию (логгер работает дальше).
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

    // --------------------------------------------------------------------------------------------
    // Инициализация
    // --------------------------------------------------------------------------------------------

    void initLocked(LoggerState& state, const Config& cfg) {
        // Оптимизация: если конфиг не изменился, не переоткрываем файл.
        if (state.initialized && state.config == cfg) {
            return;
        }

        // Закрываем старый файл, если был открыт.
        if (state.file.is_open()) {
            state.file.flush();
            state.file.close();
        }

        state.config = cfg;
        openLogFileLocked(state, cfg);

        // Fast-path гейт: после применения конфига макросы могут отсекать
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
        // Предполагается, что вызывающий код уже держит state.mutex.
        if (state.initialized) {
            return;
        }
        initLocked(state, defaults::makeDefaultConfig());
    }

} // anonymous namespace

    // --------------------------------------------------------------------------------------------
    // Публичный API — жизненный цикл
    // --------------------------------------------------------------------------------------------

    void init(const Config& config) {
        auto& state = getState();
        std::lock_guard lock(state.mutex);
        initLocked(state, config);
    }

    void setMinLevel(Level level) {
        auto& state = getState();
        std::lock_guard lock(state.mutex);

        ensureInitialized(state);
        state.config.minLevel = level;

        // Обновляем fast-path гейт.
        g_minLevelAtomic.store(level, std::memory_order_release);
        g_fastGateReady.store(true, std::memory_order_release);
    }

    void shutdown() {
        auto& state = getState();
        std::lock_guard lock(state.mutex);

        if (!state.initialized) {
            return;
        }

        if (state.file.is_open()) {
            state.file.flush();
            state.file.close();
        }

        state.initialized = false;

        // Возвращаем «консервативный режим»: до следующей инициализации не делаем early-reject.
        g_fastGateReady.store(false, std::memory_order_release);
    }

    // --------------------------------------------------------------------------------------------
    // Путь с готовым сообщением
    // --------------------------------------------------------------------------------------------

    void log(Level level,
             std::string_view category,
             std::string_view message,
             const char* file,
             int line) {
        // Атомарный fast-gate (эта функция может вызываться без макро-гейта).
        if (g_fastGateReady.load(std::memory_order_acquire)) {
            if (level < g_minLevelAtomic.load(std::memory_order_relaxed)) {
                return;
            }
        }

        auto& state = getState();
        std::lock_guard lock(state.mutex);

        ensureInitialized(state);

        // Точная проверка под mutex: гарантирует, что сообщения не «пробьются»
        // при гонке с setMinLevel().
        if (level < state.config.minLevel) {
            return;
        }

        auto& buf = state.scratch;
        buildEnvelopePrefix(buf, level, category);
        buf.append(message.data(), message.size());
        buildEnvelopeSuffix(buf, file, line);

        emitLine(state, level);
    }

    // --------------------------------------------------------------------------------------------
    // Panic (готовое сообщение)
    // --------------------------------------------------------------------------------------------

    [[noreturn]] void panic(std::string_view category,
                            std::string_view message,
                            const char* file,
                            int line) {
        // Пишем в лог как CRITICAL.
        log(Level::Critical, category, message, file, line);

        // Сбрасываем и закрываем файл.
        auto& state = getState();
        {
            std::lock_guard lock(state.mutex);
            if (state.file.is_open()) {
                state.file.flush();
                state.file.close();
            }
            state.initialized = false;
            g_fastGateReady.store(false, std::memory_order_release);
        }

        // Собираем текст для пользователя.
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
        std::cerr << "\n========================================\n"
                  << "КРИТИЧЕСКАЯ ОШИБКА\n\n"
                  << userMessage << '\n'
                  << "========================================\n\n";
    #endif

        std::exit(EXIT_FAILURE);
    }

    // --------------------------------------------------------------------------------------------
    // Форматируемый путь — non-template sink'и
    // --------------------------------------------------------------------------------------------

    namespace detail {

        void log_write(Level level,
                       std::string_view category,
                       const char* file,
                       int line,
                       std::string_view fmt,
                       std::format_args args) {
            // Атомарный fast-gate (избыточен при вызове из макроса, но безопасен для всех путей).
            if (g_fastGateReady.load(std::memory_order_acquire)) {
                if (level < g_minLevelAtomic.load(std::memory_order_relaxed)) {
                    return;
                }
            }

            auto& state = getState();
            std::lock_guard lock(state.mutex);

            ensureInitialized(state);

            if (level < state.config.minLevel) {
                return;
            }

            auto& buf = state.scratch;
            buildEnvelopePrefix(buf, level, category);

        #ifdef _DEBUG
            try {
                std::vformat_to(std::back_inserter(buf), fmt, args);
            } catch (const std::format_error& e) {
                // Аварийный emit: без std::format, без рекурсии в логгер.
                buf += "[FORMAT ERROR: ";
                buf += e.what();
                buf += " | fmt: ";
                buf.append(fmt.data(), fmt.size());
                buf += ']';
            } catch (...) {
                buf += "[UNEXPECTED ERROR during log formatting]";
            }
        #else
            std::vformat_to(std::back_inserter(buf), fmt, args);
        #endif

            buildEnvelopeSuffix(buf, file, line);

            emitLine(state, level);
        }

        [[noreturn]] void panic_write(std::string_view category,
                                      const char* file,
                                      int line,
                                      std::string_view fmt,
                                      std::format_args args) {
            // Форматируем payload, затем делегируем в panic(), который логирует и завершает процесс.
            // Это crash-path — одна лишняя строка допустима.
            std::string message;
        #ifdef _DEBUG
            try {
                message = std::vformat(fmt, args);
            } catch (const std::format_error& e) {
                message = "[PANIC FORMAT ERROR: ";
                message += e.what();
                message += " | fmt: ";
                message.append(fmt.data(), fmt.size());
                message += ']';
            } catch (...) {
                message = "[UNEXPECTED ERROR during panic formatting]";
            }
        #else
            message = std::vformat(fmt, args);
        #endif

            panic(category, message, file, line);
        }

    } // namespace detail

} // namespace core::log