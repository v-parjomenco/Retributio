// ================================================================================================
// File: core/debug/scoped_timer.h
// Purpose: RAII scoped timer — measures wall-clock elapsed time and emits LOG_DEBUG on
//          destruction. Intended for coarse-grained timing of init sequences and sessions.
// Used by: Entry point mains (main_atrapacielos, main_spatial_harness, main_render_stress),
//          stress harnesses, any code needing lightweight elapsed-time instrumentation.
// Related: core/log/log_macros.h, core/log/log_categories.h
// Notes:
//  - Non-copyable, non-movable: RAII ownership; moving a live timer has no useful semantics.
//  - Label is std::string_view — caller must ensure the pointed-to string outlives the timer.
//    String literals are the canonical use case (zero allocation, guaranteed lifetime).
//  - Destructor is noexcept: logging call is wrapped in try/catch to prevent propagation.
//  - In Release builds LOG_DEBUG may be compiled out entirely; (void)elapsed suppresses C4189.
// ================================================================================================
#pragma once

#include <chrono>
#include <string_view>
#include <type_traits>

#include "core/log/log_categories.h"
#include "core/log/log_macros.h"

namespace core::debug {

    class ScopedTimer {
    public:
        /// Начинает отсчёт времени. label должен пережить объект (строковый литерал — идеально).
        explicit ScopedTimer(std::string_view label) noexcept
            : label_(label)
            , start_(std::chrono::steady_clock::now())
        {}

        /// Останавливает отсчёт и логирует elapsed в миллисекундах через LOG_DEBUG.
        /// noexcept: вызов логгера обёрнут в try/catch — деструктор никогда не бросает.
        ~ScopedTimer() noexcept {
            try {
                const auto end     = std::chrono::steady_clock::now();
                const auto elapsed =
                    std::chrono::duration_cast<std::chrono::milliseconds>(end - start_).count();
                LOG_DEBUG(core::log::cat::Performance,
                          "[TIMER] {} took {} ms", label_, elapsed);
                // Подавляет C4189 в Release, где LOG_DEBUG может быть вырезан препроцессором.
                (void)elapsed;
            } catch (...) {
                // Исключение из логгера не должно покидать деструктор.
            }
        }

        ScopedTimer(const ScopedTimer&)            = delete;
        ScopedTimer& operator=(const ScopedTimer&) = delete;
        ScopedTimer(ScopedTimer&&)                 = delete;
        ScopedTimer& operator=(ScopedTimer&&)      = delete;

    private:
        std::string_view                       label_;
        std::chrono::steady_clock::time_point  start_;
    };

} // namespace core::debug

// Компиляционные гарантии семантики владения. Нарушение = ошибка сборки.
static_assert(!std::is_copy_constructible_v<core::debug::ScopedTimer>);
static_assert(!std::is_copy_assignable_v<core::debug::ScopedTimer>);
static_assert(!std::is_move_constructible_v<core::debug::ScopedTimer>);
static_assert(!std::is_move_assignable_v<core::debug::ScopedTimer>);
static_assert(std::is_nothrow_destructible_v<core::debug::ScopedTimer>);
