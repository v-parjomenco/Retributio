// ================================================================================================
// File: core/runtime/entry/working_directory.h
// Purpose: Sets the process working directory to the project root via platform-native
//          executable path resolution and sentinel-file probing.
// Used by: Entry point mains (main_atrapacielos, main_spatial_harness, main_render_stress).
// Notes:
//  - Uses GetModuleFileNameW (Windows), /proc/self/exe (Linux),
//    _NSGetExecutablePath + realpath (macOS). argv[0] is intentionally NOT used.
//  - Sentinel probing is OS-level (GetFileAttributesW / stat) to avoid std::filesystem
//    allocations and exceptions in the entry layer.
//  - A directory is accepted as root only if ALL provided sentinel files exist within it.
//  - The function is noexcept: all errors are handled via return value; no exceptions escape.
// ================================================================================================
#pragma once

#include <span>
#include <string_view>

namespace core::runtime::entry {

    /// Устанавливает рабочий каталог процесса в корень проекта, найденный подъёмом по дереву
    /// от директории исполняемого файла. Корень считается валидным, если в нём присутствуют
    /// все sentinel-файлы из переданного списка.
    ///
    /// @param sentinels  Непустой список путей относительно кандидата (без ведущего слеша),
    ///                   например "assets/config/engine_settings.json".
    ///                   Все файлы должны существовать, иначе кандидат отвергается.
    /// @returns true если корень найден и CWD успешно установлен.
    ///          false если корень не найден или установка не удалась (side effects отсутствуют).
    [[nodiscard]] bool setWorkingDirectoryFromExecutable(
        std::span<const std::string_view> sentinels) noexcept;

} // namespace core::runtime::entry