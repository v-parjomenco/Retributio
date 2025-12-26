#include "pch.h"
#include "core/utils/file_loader.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <string_view>
#include <vector>

#include "core/log/log_macros.h"

namespace core::utils {

    namespace {
        
        // ----------------------------------------------------------------------------------------
        // Безопасное получение размера файла через stream seeking
        // ----------------------------------------------------------------------------------------
        [[nodiscard]] std::optional<std::size_t> getStreamSize(
            std::ifstream& ifs,
            [[maybe_unused]] const std::string& path,
            [[maybe_unused]] std::string_view what) {
            ifs.seekg(0, std::ios::end);
            if (!ifs) {
                LOG_DEBUG(core::log::cat::Engine,
                          "[FileLoader] Не удалось перейти в конец {}: {}",
                          what, path);
                return std::nullopt;
            }

            const std::streampos endPosition = ifs.tellg();
            if (endPosition < 0) {
                LOG_DEBUG(core::log::cat::Engine,
                          "[FileLoader] Не удалось определить размер {}: {}",
                          what, path);
                return std::nullopt;
            }

            return static_cast<std::size_t>(endPosition);
        }

        // ----------------------------------------------------------------------------------------
        // Безопасный возврат в начало файла
        // ----------------------------------------------------------------------------------------
        [[nodiscard]] bool seekToBegin(std::ifstream& ifs,
                                       [[maybe_unused]] const std::string& path,
                                       [[maybe_unused]] std::string_view what) {
            ifs.seekg(0, std::ios::beg);
            if (!ifs) {
                LOG_DEBUG(core::log::cat::Engine,
                          "[FileLoader] Не удалось перейти в начало {}: {}",
                          what, path);
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
                                     [[maybe_unused]] const std::string& path,
                                     [[maybe_unused]] std::string_view what) {
            if (size == 0) {
                return true;
            }

            ifs.read(static_cast<char*>(dst), static_cast<std::streamsize>(size));

            const auto readCount = static_cast<std::size_t>(ifs.gcount());
            if (readCount != size) {
                LOG_DEBUG(core::log::cat::Engine,
                          "[FileLoader] Прочитано меньше байт, чем ожидалось ({}): {}",
                          what, path);
                return false;
            }

            if (ifs.bad()) {
                LOG_DEBUG(core::log::cat::Engine,
                          "[FileLoader] Ошибка чтения ({}): {}",
                          what, path);
                return false;
            }

            return true;
        }

    } // namespace

    std::optional<std::string> FileLoader::loadTextFile(const std::string& path) {
        std::ifstream ifs(path, std::ios::in | std::ios::binary);
        if (!ifs) {
            LOG_DEBUG(core::log::cat::Engine, "[FileLoader] Не удалось открыть файл: {}", path);
            return std::nullopt;
        }

        const auto sizeOpt = getStreamSize(ifs, path, "файла");
        if (!sizeOpt) {
            return std::nullopt;
        }

        if (!seekToBegin(ifs, path, "файла")) {
            return std::nullopt;
        }

        std::string content;
        content.resize(*sizeOpt);

        if (!readExact(ifs, content.data(), *sizeOpt, path, "файл")) {
            return std::nullopt;
        }

        return content;
    }

    std::optional<std::vector<std::uint8_t>> FileLoader::loadBinaryFile(const std::string& path) {
        std::ifstream ifs(path, std::ios::in | std::ios::binary);
        if (!ifs) {
            LOG_DEBUG(core::log::cat::Engine, "[FileLoader] Не удалось открыть бинарный файл: {}", path);
            return std::nullopt;
        }

        const auto sizeOpt = getStreamSize(ifs, path, "бинарного файла");
        if (!sizeOpt) {
            return std::nullopt;
        }

        if (!seekToBegin(ifs, path, "бинарного файла")) {
            return std::nullopt;
        }

        std::vector<std::uint8_t> data(*sizeOpt);
        if (!readExact(ifs, data.data(), *sizeOpt, path, "бинарный файл")) {
            return std::nullopt;
        }

        return data;
    }

    bool FileLoader::fileExists(const std::string& path) {
        std::error_code ec;
        const bool exists = std::filesystem::exists(path, ec);
        return !ec && exists;
    }

    bool FileLoader::isReadable(const std::string& path) {
        std::ifstream ifs(path, std::ios::in | std::ios::binary);
        return static_cast<bool>(ifs);
    }

} // namespace core::utils