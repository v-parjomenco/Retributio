// ============================================================================
// file_loader.h - Low-level file I/O utilities
// Centralized file loading for the entire engine.
// All file I/O should go through this module to:
// - Enable future async loading
// - Add error handling in one place
// - Support virtual file systems later (for modding)
// ============================================================================

#pragma once

#include <optional>
#include <string>
#include <vector>

namespace core::utils {

/**
 * @brief Низкоуровневый загрузчик
 * 
 * Управляет загрузкой файлов для движка.
 * Никакой иной логики, кроме чтения сырых файлов здесь быть не должно.
 */
class FileLoader {
public:
    // Текстовые файлы (JSON, конфиги, скрипты)
    static std::optional<std::string> loadTextFile(const std::string& path);
    
    // Бинарные файлы (изображения, звуки, произвольные данные)
    static std::optional<std::vector<uint8_t>> loadBinaryFile(const std::string& path);
    
    // Утилиты
    static bool fileExists(const std::string& path);
    static bool isReadable(const std::string& path);
};

} // namespace core::utils