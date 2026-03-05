// ================================================================================================
// file_loader.h - Low-level file I/O utilities
// Centralized file loading for the entire engine.
// All file I/O should go through this module to:
// - Enable future async loading
// - Add error handling in one place
// - Support virtual file systems later (for modding)
// ================================================================================================
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace core::utils {

    /**
     * @brief Низкоуровневый загрузчик файлов.
     *
     * Ответственность:
     *  - читать текстовые и бинарные файлы как сырые байты;
     *  - логировать только низкоуровневые I/O-проблемы
     *    (категория Engine, уровень DEBUG);
     *  - не решать, является ли ошибка критичной — это
     *    обязанность вызывающих уровней (лоадеры конфигов,
     *    ресурсный слой и т.п.).
     *
     * Никакой иной логики, кроме чтения сырых файлов, здесь быть не должно.
     */
    class FileLoader {
      public:
        // Текстовые файлы (JSON, конфиги, скрипты)
        [[nodiscard]] static std::optional<std::string>
        loadTextFile(const std::string& path);

        // Бинарные файлы (изображения, звуки, произвольные данные)
        [[nodiscard]] static std::optional<std::vector<std::uint8_t>>
        loadBinaryFile(const std::string& path);

        // Утилиты
        [[nodiscard]] static bool fileExists(const char* path);
        [[nodiscard]] static bool isReadable(const char* path);
        [[nodiscard]] static bool fileExists(std::string_view path);
        [[nodiscard]] static bool isReadable(std::string_view path);
        [[nodiscard]] static bool fileExists(const std::string& path);
        [[nodiscard]] static bool isReadable(const std::string& path);

        // Чтение (filesystem::path)
        [[nodiscard]] static std::optional<std::string>
        loadTextFile(const std::filesystem::path& path);

        [[nodiscard]] static std::optional<std::vector<std::uint8_t>>
        loadBinaryFile(const std::filesystem::path& path);

        [[nodiscard]] static bool fileExists(const std::filesystem::path& path);
        [[nodiscard]] static bool isReadable(const std::filesystem::path& path);

        // Атомарная запись
        [[nodiscard]] static bool writeTextFileAtomic(const std::filesystem::path& path,
                                                      std::string_view content) noexcept;
        [[nodiscard]] static bool writeTextFileAtomic(const std::string& path,
                                                      std::string_view content) noexcept;
    };

} // namespace core::utils