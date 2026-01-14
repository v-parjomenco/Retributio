#include "pch.h"
#include "core/utils/file_loader.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string_view>
#include <system_error>
#include <vector>

#include "core/log/log_macros.h"

#if defined(_WIN32)
    #include <Windows.h>
#endif

namespace core::utils {

    namespace {
        namespace fs = std::filesystem;

        // ----------------------------------------------------------------------------------------
        // Безопасное получение размера файла через stream seeking
        // ----------------------------------------------------------------------------------------
        [[nodiscard]] std::optional<std::size_t> getStreamSize(
            std::ifstream& ifs,
            [[maybe_unused]] std::string_view pathForLog,
            [[maybe_unused]] std::string_view what) {
            ifs.seekg(0, std::ios::end);
            if (!ifs) {
                LOG_DEBUG(core::log::cat::Engine,
                          "[FileLoader] Не удалось перейти в конец {}: {}",
                          what, pathForLog);
                return std::nullopt;
            }

            const std::streampos endPosition = ifs.tellg();
            if (endPosition < 0) {
                LOG_DEBUG(core::log::cat::Engine,
                          "[FileLoader] Не удалось определить размер {}: {}",
                          what, pathForLog);
                return std::nullopt;
            }

            return static_cast<std::size_t>(endPosition);
        }

        // ----------------------------------------------------------------------------------------
        // Безопасный возврат в начало файла
        // ----------------------------------------------------------------------------------------
        [[nodiscard]] bool seekToBegin(std::ifstream& ifs,
                                       [[maybe_unused]] std::string_view pathForLog,
                                       [[maybe_unused]] std::string_view what) {
            ifs.seekg(0, std::ios::beg);
            if (!ifs) {
                LOG_DEBUG(core::log::cat::Engine,
                          "[FileLoader] Не удалось перейти в начало {}: {}",
                          what, pathForLog);
                return false;
            }
            return true;
        }

        // ----------------------------------------------------------------------------------------
        // Чтение точного количества байт в буфер
        // ----------------------------------------------------------------------------------------
        [[nodiscard]] bool readExact(std::ifstream& ifs,
                                     void* dst,
                                     std::size_t size,
                                     [[maybe_unused]] std::string_view pathForLog,
                                     [[maybe_unused]] std::string_view what) {
            if (size == 0) {
                return true;
            }

            ifs.read(static_cast<char*>(dst), static_cast<std::streamsize>(size));

            const auto readCount = static_cast<std::size_t>(ifs.gcount());
            if (readCount != size) {
                LOG_DEBUG(core::log::cat::Engine,
                          "[FileLoader] Прочитано меньше байт, чем ожидалось ({}): {}",
                          what, pathForLog);
                return false;
            }

            if (ifs.bad()) {
                LOG_DEBUG(core::log::cat::Engine,
                          "[FileLoader] Ошибка чтения ({}): {}",
                          what, pathForLog);
                return false;
            }

            return true;
        }

        [[nodiscard]] fs::path makeTempSiblingPath(const fs::path& target) {
            const auto nowNs =
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();

            fs::path tmp = target;
            tmp += ".tmp.";
            tmp += std::to_string(nowNs);
            return tmp;
        }

        [[nodiscard]] bool ensureParentDirExists(const fs::path& path) noexcept {
            std::error_code ec;
            const fs::path dir = path.parent_path();
            if (dir.empty()) {
                return true;
            }

            fs::create_directories(dir, ec);
            if (ec) {
                LOG_DEBUG(core::log::cat::Engine,
                          "[FileLoader] Не удалось создать директорию '{}': {}",
                          dir.string(),
                          ec.message());
                return false;
            }
            return true;
        }

        [[nodiscard]] bool writeTextFile(
            const fs::path& path, const std::string_view text) noexcept {
            std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
            if (!ofs) {
                return false;
            }

            ofs.write(text.data(), static_cast<std::streamsize>(text.size()));
            if (!ofs) {
                return false;
            }

            ofs.flush();
            return static_cast<bool>(ofs);
        }

        [[nodiscard]] bool atomicReplaceFile(const fs::path& tmp, const fs::path& target) noexcept {
#if defined(_WIN32)
            const std::wstring tmpW = tmp.wstring();
            const std::wstring dstW = target.wstring();

            if (::MoveFileExW(tmpW.c_str(),
                              dstW.c_str(),
                              MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0) {
                return true;
            }

            const DWORD err = ::GetLastError();
            (void) err;
            LOG_DEBUG(core::log::cat::Engine,
                      "[FileLoader] MoveFileExW failed (error={}): '{}' -> '{}'",
                      static_cast<unsigned long>(err),
                      tmp.string(),
                      target.string());
            return false;
#else
            std::error_code ec;
            fs::rename(tmp, target, ec);
            if (!ec) {
                return true;
            }

            LOG_DEBUG(core::log::cat::Engine,
                      "[FileLoader] rename failed: '{}' -> '{}' ({})",
                      tmp.string(),
                      target.string(),
                      ec.message());
            return false;
#endif
        }

    } // namespace

    // Перегрузки string -> path

    std::optional<std::string> FileLoader::loadTextFile(const std::string& path) {
        return loadTextFile(std::filesystem::path(path));
    }

    std::optional<std::vector<std::uint8_t>> FileLoader::loadBinaryFile(const std::string& path) {
        return loadBinaryFile(std::filesystem::path(path));
    }

    bool FileLoader::fileExists(const std::string& path) {
        return fileExists(std::filesystem::path(path));
    }

    bool FileLoader::isReadable(const std::string& path) {
        return isReadable(std::filesystem::path(path));
    }

    bool FileLoader::writeTextFileAtomic(const std::string& path,
                                         std::string_view content) noexcept {
        return writeTextFileAtomic(std::filesystem::path(path), content);
    }

    // Перегрузки path

    std::optional<std::string> FileLoader::loadTextFile(const std::filesystem::path& path) {
        std::ifstream ifs(path, std::ios::in | std::ios::binary);
        if (!ifs) {
            LOG_DEBUG(core::log::cat::Engine,
                      "[FileLoader] Не удалось открыть файл: {}",
                      path.string());
            return std::nullopt;
        }

        const std::string pathForLog = path.string();

        const auto sizeOpt = getStreamSize(ifs, pathForLog, "файла");
        if (!sizeOpt) {
            return std::nullopt;
        }

        if (!seekToBegin(ifs, pathForLog, "файла")) {
            return std::nullopt;
        }

        std::string content;
        content.resize(*sizeOpt);

        if (!readExact(ifs, content.data(), *sizeOpt, pathForLog, "файл")) {
            return std::nullopt;
        }

        return content;
    }

    std::optional<std::vector<std::uint8_t>> FileLoader::loadBinaryFile(
        const std::filesystem::path& path) {
        std::ifstream ifs(path, std::ios::in | std::ios::binary);
        if (!ifs) {
            LOG_DEBUG(core::log::cat::Engine,
                      "[FileLoader] Не удалось открыть бинарный файл: {}",
                      path.string());
            return std::nullopt;
        }

        const std::string pathForLog = path.string();

        const auto sizeOpt = getStreamSize(ifs, pathForLog, "бинарного файла");
        if (!sizeOpt) {
            return std::nullopt;
        }

        if (!seekToBegin(ifs, pathForLog, "бинарного файла")) {
            return std::nullopt;
        }

        std::vector<std::uint8_t> data(*sizeOpt);
        if (!readExact(ifs, data.data(), *sizeOpt, pathForLog, "бинарный файл")) {
            return std::nullopt;
        }

        return data;
    }

    bool FileLoader::fileExists(const std::filesystem::path& path) {
        std::error_code ec;
        const bool exists = std::filesystem::exists(path, ec);
        return !ec && exists;
    }

    bool FileLoader::isReadable(const std::filesystem::path& path) {
        std::ifstream ifs(path, std::ios::in | std::ios::binary);
        return static_cast<bool>(ifs);
    }

    bool FileLoader::writeTextFileAtomic(const std::filesystem::path& path,
                                         std::string_view content) noexcept {
        if (!ensureParentDirExists(path)) {
            return false;
        }

        const fs::path tmp = makeTempSiblingPath(path);

        if (!writeTextFile(tmp, content)) {
            LOG_DEBUG(core::log::cat::Engine,
                      "[FileLoader] Не удалось записать tmp-файл: {}",
                      tmp.string());
            std::error_code ec;
            fs::remove(tmp, ec);
            return false;
        }

        if (!atomicReplaceFile(tmp, path)) {
            std::error_code ec;
            fs::remove(tmp, ec);
            return false;
        }

        return true;
    }

} // namespace core::utils