// ================================================================================================
// File: tests/engine/include/stable_key_seam.h
// Purpose: Test-only API for injecting a hash override into the StableKey64 seam.
//
// Include ONLY from test TUs that are compiled into retributio_engine_tests.
// NEVER include from retributio_core or any production target — this header has no meaning
// outside the test seam link unit (stable_key_impl_seam.cpp).
//
// Usage pattern:
//   core::resources::detail::setHashOverride(
//       [](std::string_view) noexcept -> std::uint64_t { return kConstant; });
//
//   // ... test body that triggers loadFromSources ...
//
//   core::resources::detail::setHashOverride(nullptr);  // always reset
//
// Use an RAII guard (see ScopedHashOverride below) to ensure the override is always
// cleared even if the test throws (e.g., via panic → std::runtime_error).
// ================================================================================================
#pragma once

#include <cstdint>
#include <string_view>

namespace core::resources::detail {

    using HashOverrideFn = std::uint64_t (*)(std::string_view) noexcept;

    /// Устанавливает process-wide переопределение хэш-функции для тестирования.
    /// Передай nullptr, чтобы восстановить поведение по умолчанию (xxHash3_64).
    ///
    /// Thread safety: seq_cst-запись. Безопасно вызывать из тест-потока до
    /// загрузки реестра и сбрасывать из catch-блока или деструктора RAII.
    void setHashOverride(HashOverrideFn fn) noexcept;

    // --------------------------------------------------------------------------------------------
    // RAII-guard — гарантирует сброс переопределения при выходе из scope,
    // включая выход через исключение.
    // --------------------------------------------------------------------------------------------
    struct ScopedHashOverride final {
        explicit ScopedHashOverride(HashOverrideFn fn) noexcept {
            setHashOverride(fn);
        }
        ~ScopedHashOverride() noexcept {
            setHashOverride(nullptr);
        }
        ScopedHashOverride(const ScopedHashOverride&)            = delete;
        ScopedHashOverride& operator=(const ScopedHashOverride&) = delete;
    };

} // namespace core::resources::detail