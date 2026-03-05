// ================================================================================================
// File: core/runtime/entry/single_instance_guard.h
// Purpose: Prevents multiple concurrent instances of the same executable via OS primitives.
// Used by: main_atrapacielos.cpp (game entry point requiring exclusive launch).
// Related: runtime/entry/user_dialog.h (caller shows error based on status())
// Notes:
//  - Move-only: resource ownership is exclusive; copying is meaningless for OS handles.
//  - Moved-from state is Inactive: destructor is a guaranteed no-op.
//  - Destructor is noexcept: OS release calls (CloseHandle, flock/close) do not throw.
//
//  Windows policy (Global\ namespace):
//   - Uses CreateMutexW with the name "Global\Atrapacielos_Game_Mutex".
//   - Global\ namespace requires no elevated privileges on standard desktop (Vista+).
//   - Cross-session protection: one user cannot run two instances across RDS sessions.
//   - ERROR_ALREADY_EXISTS → AlreadyRunning (another instance holds the mutex).
//   - ERROR_ACCESS_DENIED and all other API failures → OsError (fail-fast; no Local\ fallback).
//   - No Local\ fallback: silent fallback would mask double-launch in RDS scenarios.
//
//  Unix/macOS policy (flock on temp file):
//   - Uses an exclusive flock on a file in the temp directory ("atrapacielos.lock").
//   - flock is POSIX; implementation is shared between Linux and macOS.
//   - Lock file is intentionally not removed on exit to avoid an unlink/open race.
//   - open() failure → OsError. EWOULDBLOCK from flock → AlreadyRunning.
// ================================================================================================
#pragma once

#include <type_traits>

#ifdef _WIN32
    // WIN32_LEAN_AND_MEAN и NOMINMAX установлены в core/compiler/platform/windows.h
    // (через pch.h). HANDLE — struct-pointer тип (STRICT по умолчанию в Windows SDK),
    // не void*. Публичный заголовок не может полагаться на PCH для типов в теле
    // класса — включаем <windows.h> явно, как делает SFML в WindowHandle.hpp.
    #include <windows.h>
#endif

namespace core::runtime::entry {

    enum class SingleInstanceStatus {
        Acquired,       ///< Lock acquired. This is the sole running instance.
        AlreadyRunning, ///< Lock held by another instance. Caller should exit gracefully.
        OsError,        ///< OS primitive failed unexpectedly. Caller should fail-fast.
        Inactive,       ///< Moved-from state. Destructor is a guaranteed no-op.
    };

    class SingleInstanceGuard {
    public:
        /// Пытается захватить системную блокировку.
        /// Статус результата доступен через status().
        SingleInstanceGuard() noexcept;
        ~SingleInstanceGuard() noexcept;

        // Move-only: передача владения OS-ресурсом без его освобождения и захвата повторно.
        SingleInstanceGuard(SingleInstanceGuard&& other) noexcept;
        SingleInstanceGuard& operator=(SingleInstanceGuard&& other) noexcept;

        // Копирование удалено: один OS-примитив не может принадлежать двум объектам.
        SingleInstanceGuard(const SingleInstanceGuard&)            = delete;
        SingleInstanceGuard& operator=(const SingleInstanceGuard&) = delete;

        [[nodiscard]] SingleInstanceStatus status() const noexcept;

    private:
        // Освобождает удерживаемый OS-ресурс и переводит объект в состояние Inactive.
        // Безопасно вызывать в любом состоянии, включая уже Inactive.
        void release() noexcept;

        SingleInstanceStatus status_ = SingleInstanceStatus::OsError;

#ifdef _WIN32
        HANDLE mutex_ = nullptr;
#else
        int fd_ = -1;
        // lockPath_ не хранится как поле — в деструкторе нужен только fd_.
        // Путь вычисляется локально в конструкторе и отбрасывается после open().
#endif
    };

} // namespace core::runtime::entry

// Компиляционные гарантии семантики владения — нарушение = ошибка сборки.
static_assert(!std::is_copy_constructible_v<core::runtime::entry::SingleInstanceGuard>);
static_assert(!std::is_copy_assignable_v<core::runtime::entry::SingleInstanceGuard>);
static_assert(std::is_nothrow_move_constructible_v<core::runtime::entry::SingleInstanceGuard>);
static_assert(std::is_nothrow_move_assignable_v<core::runtime::entry::SingleInstanceGuard>);
static_assert(std::is_nothrow_destructible_v<core::runtime::entry::SingleInstanceGuard>);